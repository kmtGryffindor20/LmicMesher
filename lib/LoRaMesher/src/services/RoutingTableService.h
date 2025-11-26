#ifndef _LORAMESHER_ROUTING_TABLE_SERVICE_H
#define _LORAMESHER_ROUTING_TABLE_SERVICE_H

#include "utilities/LinkedQueue.hpp"
#include "entities/routingTable/RouteNode.h"
#include "entities/routingTable/NetworkNode.h"
#include "entities/packets/RoutePacket.h"
#include "BuildOptions.h"
#include "services/WiFiService.h"
#include "services/RoleService.h"

/**
 * @brief Routing Table Service with SNR Hysteresis + Reroute-on-New-Node
 */
class RoutingTableService {
public:

    /** Routing table list */
    static LM_LinkedList<RouteNode>* routingTableList;

    /** Print routing table */
    static void printRoutingTable();

    /** Get list of all network nodes */
    static NetworkNode* getAllNetworkNodes();

    /** Find route-node by address */
    static RouteNode* findNode(uint16_t address);

    /** Get nearest node providing the given role */
    static RouteNode* getBestNodeByRole(uint8_t role);

    /** Check if routing table contains address */
    static bool hasAddressRoutingTable(uint16_t address);

    /** Get next hop for destination */
    static uint16_t getNextHop(uint16_t dst);

    /** Get hop count for an address */
    static uint8_t getNumberOfHops(uint16_t address);

    /** Get routing table size */
    static size_t routingTableSize();

    /**
     * @brief Process route packet (SNR + regular)
     */
    static void processRoute(RoutePacket* p, int8_t receivedSNR);

    /**
     * @brief Process route packet and return whether routing table updated
     */
    static void processRoute(RoutePacket* p, int8_t receivedSNR, bool& routingTableUpdated);

    /** Reset SNR for received route */
    static void resetReceiveSNRRoutePacket(uint16_t src, int8_t receivedSNR);

    /** Reset SNR for sent route */
    static void resetSentSNRRoutePacket(uint16_t src, int8_t sentSNR);

    /** Remove timed-out routes */
    static void manageTimeoutRoutingTable();

    // =====================================================
    //      SNR Hysteresis + Stability Evaluation API
    // =====================================================

    /**
     * @brief Decide if route should change based on SNR + hops
     */
    static bool shouldChangeRoute(uint16_t currentVia, uint8_t currentMetric, int8_t currentSNR,
                                  uint16_t candidateVia, uint8_t candidateMetric, int8_t candidateSNR);

    /** Update exponential moving SNR average */
    static void updateSNRAverage(RouteNode* rNode, int8_t newSNR);

    /** Check if a candidate route has been stable long enough */
    static bool isRouteChangeStable(RouteNode* rNode);

    /** Check if enough time passed to reevaluate route */
    static bool canReevaluateRoute(RouteNode* rNode);

    /** Check SNR threshold for **new** routes */
    static bool isSNRAcceptable(int8_t snr);

    /** Check SNR threshold for **existing** routes with hysteresis margin */
    static bool isExistingRouteSNRAcceptable(int8_t avgSNR);

    /** Check if route should be removed because SNR is consistently poor */
    static bool shouldRemoveRouteDueToSNR(RouteNode* rNode, int8_t currentSNR);

    /** Remove all consistently weak links */
    static void removeWeakRoutes();


private:

    // =====================================================
    //              ROUTE PROCESSING INTERNALS
    // =====================================================

    /**
     * @brief Process network node entry (basic + SNR aware)
     */
    static void processRoute(uint16_t via, NetworkNode* node, int8_t receivedSNR);

    /**
     * @brief Reroute-on-new-node path that returns update flag
     */
    static void processRoute(uint16_t via, NetworkNode* node, int8_t receivedSNR, bool& routingTableUpdated);

    /**
     * @brief Process route for existing RouteNode
     */
    static void processRoute(RouteNode* rNode, uint16_t via, NetworkNode* node, int8_t receivedSNR);

    /** Reset timeout for node */
    static void resetTimeoutRoutingNode(RouteNode* node);

    /** Add new entry to routing table */
    static void addNodeToRoutingTable(NetworkNode* node, uint16_t via);

    /** Compute max metric allowed */
    static uint8_t calculateMaximumMetricOfRoutingTable();


    // =====================================================
    //              SNR Hysteresis Constants
    // =====================================================

    /** Number of consistent good readings needed to switch */
    static const uint8_t ROUTE_STABILITY_THRESHOLD = 10;

    /** Minimum dB advantage required to switch routes with same hop-count */
    static const int8_t SNR_HYSTERESIS_MARGIN = 4;

    /** Lockout time after route change (ms) */
    static const uint32_t ROUTE_LOCKOUT_TIME = 30000; 
};

#endif
