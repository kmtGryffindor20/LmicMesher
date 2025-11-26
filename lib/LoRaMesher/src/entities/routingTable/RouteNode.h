#ifndef _LORAMESHER_ROUTE_NODE_H
#define _LORAMESHER_ROUTE_NODE_H

#include "NetworkNode.h"

#pragma pack(1)
class RouteNode {
public:
    /**
     * @brief Network node
     *
     */
    NetworkNode networkNode;

    /**
     * @brief Timeout of the route
     *
     */
    uint32_t timeout = 0;

    /**
     * @brief Next hop to send the message
     *
     */
    uint16_t via = 0;

    /**
     * @brief SNR from received packets. Only available nodes at 1 hop.
     *
     */
    int8_t receivedSNR = 0;

    /**
     * @brief SNR from sent packets. Only available nodes at 1 hop.
     *
     */
    int8_t sentSNR = 0;

    /**
     * @brief SRTT, smoothed round-trip time (RFC 6298)
     *
     */
    unsigned long SRTT = 0;

    /**
     * @brief RTTVAR, round-trip time variation (RFC 6298)
     *
     */
    unsigned long RTTVAR = 0;

    // ============ SNR Hysteresis Fields ============

    /**
     * @brief Candidate next hop for potential route switch
     *
     */
    uint16_t alternativeVia = 0;

    /**
     * @brief SNR value of the alternative via
     *
     */
    int8_t alternativeSNR = 0;

    /**
     * @brief Stability counter - tracks consecutive confirmations of candidate route
     *
     */
    uint8_t stabilityCounter = 0;

    /**
     * @brief Timestamp of last route change for lock-out mechanism
     *
     */
    uint32_t lastRouteChangeTime = 0;

    /**
     * @brief Running average SNR on current route (exponential moving average)
     *
     */
    int8_t avgSNR = 0;

    /**
     * @brief Number of SNR samples collected for averaging
     *
     */
    uint8_t sampleCount = 0;

    /**
    * @brief Counter for consecutive SNR readings below threshold
    * 
    * Tracks how many consecutive readings have been below acceptable SNR.
    * When this reaches SNR_BAD_READING_THRESHOLD, route is considered dead.
    */
    uint8_t badSNRCounter = 0;

    /**
     * @brief Construct a new Route Node object
     *
     * @param address_ Address
     * @param metric_ Metric
     * @param role_ Role
     * @param via_ Via
     */
    RouteNode(uint16_t address_, uint8_t metric_, uint8_t role_, uint16_t via_)
        : networkNode(address_, metric_, role_), via(via_) {};
};
#pragma pack()

#endif
