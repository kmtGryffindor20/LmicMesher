#include "RoutingTableService.h"

// -----------------------------------------------------------------------------
//  BASIC ROUTING TABLE ACCESSORS
// -----------------------------------------------------------------------------

size_t RoutingTableService::routingTableSize() {
    return routingTableList->getLength();
}

RouteNode* RoutingTableService::findNode(uint16_t address) {
    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->networkNode.address == address) {
                routingTableList->releaseInUse();
                return node;
            }
        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
    return nullptr;
}

RouteNode* RoutingTableService::getBestNodeByRole(uint8_t role) {
    RouteNode* bestNode = nullptr;
    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();
            if ((node->networkNode.role & role) == role &&
                (bestNode == nullptr ||
                 node->networkNode.metric < bestNode->networkNode.metric)) {
                bestNode = node;
            }
        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
    return bestNode;
}

bool RoutingTableService::hasAddressRoutingTable(uint16_t address) {
    return findNode(address) != nullptr;
}

uint16_t RoutingTableService::getNextHop(uint16_t dst) {
    RouteNode* node = findNode(dst);
    return node ? node->via : 0;
}

uint8_t RoutingTableService::getNumberOfHops(uint16_t address) {
    RouteNode* node = findNode(address);
    return node ? node->networkNode.metric : 0;
}

// -----------------------------------------------------------------------------
//  SNR FILTERING LOGIC (NEW)
// -----------------------------------------------------------------------------

bool RoutingTableService::isSNRAcceptable(int8_t snr) {
    if (snr >= SNR_MINIMUM_THRESHOLD) return true;

    ESP_LOGD(LM_TAG,
             "Reject route: SNR %d < minimum %d",
             snr, SNR_MINIMUM_THRESHOLD);
    return false;
}

void RoutingTableService::updateSNRAverage(RouteNode* rNode, int8_t newSNR) {
    if (rNode->sampleCount == 0) {
        rNode->avgSNR = newSNR;
    } else {
        rNode->avgSNR = (rNode->avgSNR * 4 + newSNR) / 5;
    }

    if (rNode->sampleCount < 10)
        rNode->sampleCount++;
}

bool RoutingTableService::shouldRemoveRouteDueToSNR(RouteNode* rNode,
                                                    int8_t currentSNR) {
    if (currentSNR >= SNR_EXISTING_ROUTE_THRESHOLD) {
        rNode->badSNRCounter = 0;
        return false;
    }

    rNode->badSNRCounter++;
    ESP_LOGW(LM_TAG,
             "Weak SNR for %X -> count %d",
             rNode->networkNode.address,
             rNode->badSNRCounter);

    return (rNode->badSNRCounter >= SNR_BAD_READING_THRESHOLD);
}


// -----------------------------------------------------------------------------
//  HYSTERESIS LOGIC FOR ROUTE SWITCHING (NEW)
// -----------------------------------------------------------------------------

bool RoutingTableService::shouldChangeRoute(
    uint16_t currentVia,
    uint8_t currentMetric,
    int8_t currentSNR,
    uint16_t candVia,
    uint8_t candMetric,
    int8_t candSNR
) {
    if (candMetric < currentMetric) return true;

    if (candMetric == currentMetric) {
        return (candSNR - currentSNR >= SNR_HYSTERESIS_MARGIN);
    }

    return false;
}

bool RoutingTableService::canReevaluateRoute(RouteNode* rNode) {
    uint32_t elapsed = millis() - rNode->lastRouteChangeTime;
    return elapsed >= ROUTE_LOCKOUT_TIME;
}


// -----------------------------------------------------------------------------
//  ROUTING PACKET PROCESSING (MERGED FROM BOTH FILES)
// -----------------------------------------------------------------------------

// Version with reroute-on-new-node support
void RoutingTableService::processRoute(RoutePacket* p,
                                       int8_t receivedSNR,
                                       bool& routingTableUpdated)
{
    if ((p->packetSize - sizeof(RoutePacket)) % sizeof(NetworkNode) != 0) {
        ESP_LOGE(LM_TAG, "Invalid route packet");
        return;
    }

    size_t numNodes = p->getNetworkNodesSize();
    ESP_LOGI(LM_TAG,
             "Route packet from %X with %d entries SNR=%d",
             p->src, numNodes, receivedSNR);

    NetworkNode* srcNode = new NetworkNode(p->src, 1, p->nodeRole);
    processRoute(p->src, srcNode, receivedSNR, routingTableUpdated);
    delete srcNode;

    resetReceiveSNRRoutePacket(p->src, receivedSNR);

    for (size_t i = 0; i < numNodes; i++) {
        NetworkNode* node = &p->networkNodes[i];
        node->metric++;
        processRoute(p->src, node, 0, routingTableUpdated);
    }

    printRoutingTable();
}


// Version without reroute flag (kept for backward compatibility)
void RoutingTableService::processRoute(RoutePacket* p, int8_t receivedSNR)
{
    bool dummy = false;
    processRoute(p, receivedSNR, dummy);
}


// -----------------------------------------------------------------------------
//  RESET SNR (MERGED)
// -----------------------------------------------------------------------------

void RoutingTableService::resetReceiveSNRRoutePacket(uint16_t src,
                                                     int8_t receivedSNR)
{
    RouteNode* rNode = findNode(src);
    if (!rNode) return;

    rNode->receivedSNR = receivedSNR;
    updateSNRAverage(rNode, receivedSNR);

    ESP_LOGD(LM_TAG,
             "Updated SNR for %X: now avg %d",
             src, rNode->avgSNR);
}


// -----------------------------------------------------------------------------
//  NODE-LEVEL ROUTE PROCESSING (FULL MERGE)
// -----------------------------------------------------------------------------

void RoutingTableService::processRoute(uint16_t via,
                                       NetworkNode* node,
                                       int8_t receivedSNR,
                                       bool& routingTableUpdated)
{
    if (node->address == WiFiService::getLocalAddress()) return;

    RouteNode* rNode = findNode(node->address);
    int8_t candidateSNR = (via == node->address ? receivedSNR : 0);

    // --- Case 1: New route (MERGED LOGIC) -----------------------------
    if (!rNode) {
        if (via == node->address && receivedSNR != 0) {
            if (!isSNRAcceptable(candidateSNR)) return;
        }

        addNodeToRoutingTable(node, via);
        routingTableUpdated = true;
        return;
    }

    // --- Update SNR for direct links ---------------------------------
    if (via == rNode->via && receivedSNR != 0) {
        updateSNRAverage(rNode, receivedSNR);

        if (shouldRemoveRouteDueToSNR(rNode, receivedSNR)) {
            routingTableList->setInUse();
            delete rNode;
            routingTableList->DeleteCurrent();
            routingTableList->releaseInUse();
            routingTableUpdated = true;
            return;
        }
    }

    // --- Hysteresis-based route selection ----------------------------
    bool trySwitch = shouldChangeRoute(
        rNode->via,
        rNode->networkNode.metric,
        rNode->avgSNR,
        via,
        node->metric,
        candidateSNR
    );

    if (trySwitch && via == node->address && receivedSNR != 0) {
        if (!isSNRAcceptable(candidateSNR))
            trySwitch = false;
    }

    if (trySwitch) {
        if (rNode->alternativeVia == via) {
            rNode->stabilityCounter++;
            if (rNode->stabilityCounter >= ROUTE_STABILITY_THRESHOLD) {
                rNode->via = via;
                rNode->networkNode.metric = node->metric;
                rNode->stabilityCounter = 0;
                rNode->alternativeVia = 0;
                rNode->badSNRCounter = 0;
                rNode->lastRouteChangeTime = millis();
                resetTimeoutRoutingNode(rNode);
                routingTableUpdated = true;
            }
        } else {
            if (canReevaluateRoute(rNode)) {
                rNode->alternativeVia = via;
                rNode->alternativeSNR = candidateSNR;
                rNode->stabilityCounter = 1;
            }
        }
    } else if (via == rNode->via &&
               node->metric == rNode->networkNode.metric)
    {
        rNode->stabilityCounter = 0;
        resetTimeoutRoutingNode(rNode);
    }

    // --- Role Update -----------------------------------------------
    if (via == rNode->via &&
        node->role != rNode->networkNode.role)
    {
        rNode->networkNode.role = node->role;
    }
}


// -----------------------------------------------------------------------------
//  ADD ROUTE
// -----------------------------------------------------------------------------

void RoutingTableService::addNodeToRoutingTable(NetworkNode* node,
                                                uint16_t via)
{
    if (routingTableList->getLength() >= RTMAXSIZE) return;
    if (calculateMaximumMetricOfRoutingTable() < node->metric) return;

    RouteNode* rNode =
        new RouteNode(node->address, node->metric, node->role, via);

    resetTimeoutRoutingNode(rNode);

    routingTableList->setInUse();
    routingTableList->Append(rNode);
    routingTableList->releaseInUse();

    ESP_LOGI(LM_TAG, "Added route %X via %X metric %d",
             node->address, via, node->metric);
}


// -----------------------------------------------------------------------------
//  ROUTING TABLE PRINTING + TIMEOUT MANAGEMENT
// -----------------------------------------------------------------------------

void RoutingTableService::printRoutingTable() {
    ESP_LOGI(LM_TAG, "=== Routing Table ===");

    routingTableList->setInUse();
    if (routingTableList->moveToStart()) {
        int idx = 0;
        do {
            RouteNode* n = routingTableList->getCurrent();

            ESP_LOGI(LM_TAG,
                "[%d] %X via %X metric %d role %d SNR(avg=%d bad=%d)",
                idx++, n->networkNode.address, n->via,
                n->networkNode.metric, n->networkNode.role,
                n->avgSNR, n->badSNRCounter
            );

        } while (routingTableList->next());
    }
    routingTableList->releaseInUse();
}

void RoutingTableService::manageTimeoutRoutingTable() {
    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* n = routingTableList->getCurrent();

            if (n->timeout < millis()) {
                delete n;
                routingTableList->DeleteCurrent();
                continue;
            }

            if (n->via == n->networkNode.address &&
                !isSNRAcceptable(n->avgSNR))
            {
                delete n;
                routingTableList->DeleteCurrent();
                continue;
            }

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
}


// -----------------------------------------------------------------------------
//  STATIC TABLE INIT
// -----------------------------------------------------------------------------

LM_LinkedList<RouteNode>* RoutingTableService::routingTableList =
    new LM_LinkedList<RouteNode>();
