/**
 * Copyright (C) 2024 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations under the License.
 *
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BACKENDS_TOFINO_BF_P4C_LOGGING_RESOURCES_H_
#define _BACKENDS_TOFINO_BF_P4C_LOGGING_RESOURCES_H_

/* clang-format off */
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ir/ir.h"
#include "ir/gress.h"
#include "lib/cstring.h"
#include "lib/ordered_map.h"
#include "resources_schema.h"
#include "bf-p4c/mau/instruction_memory.h"
#include "bf-p4c/mau/tofino/input_xbar.h"
/* clang-format on */
using Logging::Resources_Schema_Logger;
class ClotInfo;  // Forward declaration

namespace BFN {

namespace Resources {
/**
 *  XBar bytes can be shared by multiple tables for different purposes. These cases can happen:
 *  * One table can use the same data for two purposes
 *  * Two tables can use the same data for whatever purposes
 *  * Each table use different slice of the byte for different purposes, slices do not overlap
 *      * (validity bits)
 *  * Mutually exclusive tables source mutually exclusive data from the same byte
 *  * Use might be duplicated (bug possibly?) This is why this structure is held inside set and
 *    implements < operator so we can deduplicate these cases.
 */
struct XbarByteResource {
    std::string usedBy;
    std::string usedFor;
    IXBar::Use::Byte byte;

    XbarByteResource(const std::string &ub, const std::string &uf, const IXBar::Use::Byte &b);

    bool operator<(const XbarByteResource &other) const {
        return std::tie(usedBy, usedFor, byte) < std::tie(other.usedBy, other.usedFor, other.byte);
    }
};

// Each hash bit is reserved to a single table or side effect.  However, because the
// same bit can be the select bits/RAM line bit for two different ways in the same
// table, then they can be shared
struct HashBitResource {
    enum class UsageType { WaySelect = 0, WayLineSelect, SelectionBit, DistBit, Gateway };

    struct Usage {
        int value;
        std::string fieldName;
        UsageType type;

        bool operator<(const Usage &other) const {
            return std::tie(type, value, fieldName) <
                   std::tie(other.type, other.value, other.fieldName);
        }
    };

    std::string usedBy;
    std::string usedFor;
    std::set<Usage> usages;

    void append(std::string ub, std::string uf, UsageType type, int value,
                const std::string &fieldName = "");
};

// This represents the 48 bits of hash distribution before the hash distribution units
// are expanded, masked and shifted
// Hash Distribution units can be used for multiple purposes (i.e. two wide addresses
// between tables
struct HashDistResource {
    std::set<std::string> usedBy;
    std::set<std::string> usedFor;

    void append(const std::string &ub, const std::string &uf);
};

struct ActionBusByteResource {
    std::set<std::string> usedBy;

    void append(const std::string &ub);
};

struct MemoriesResource {
    const IR::MAU::Table *table;
    std::string tableName;
    std::string gatewayName;
    const TableResourceAlloc *use;

    MemoriesResource(const IR::MAU::Table *table, const std::string &name,
                     const std::string &gwname, const TableResourceAlloc *use)
        : table(table), tableName(name), gatewayName(gwname), use(use) {}
};

struct IMemColorResource {
    unsigned int color = 0;
    gress_t gress = INGRESS;

    // Key is used_by, value is list of action names
    ordered_map<std::string, std::set<std::string>> usages;
};

struct StageResources {
    ordered_map<int, cstring> logicalIds;  // Map table logical ids to table names
    ordered_map<int, std::set<XbarByteResource>> xbarBytes;
    // key is (hashBitNumber, hashFunction)
    ordered_map<std::pair<int, int>, HashBitResource> hashBits;
    // key is (hashId, unitId)
    ordered_map<std::pair<int, int>, HashDistResource> hashDist;
    ordered_map<int, ActionBusByteResource> actionBusBytes;
    ordered_map<int, std::vector<IMemColorResource>> imemColor;
    std::vector<MemoriesResource> memories;
};
}  // end namespace Resources

/**
 *  \brief Class for generating resources.json logfile
 *
 *  It uses API autogenerated from resources_schema.py, you can find it by running make
 *  and then searching for resources_schema.h in the build dir. The classes have the same
 *  names as in schema and they have the same nested structure. Dictionaries also become
 *  classes.
 *
 *  All properties that are not an array must be initialized in the constructor and parameters
 *  are always sorted alphabetically.
 *
 *  Enums will become strings/ints and optional ints/optional int enums will be int pointers
 *  (int pointer with nullptr value will not be emitted).
 */
class ResourcesLogging : public Inspector {
 public:
    using ActionDataResourceUsage = Resources_Schema_Logger::ActionDataResourceUsage;
    using ClotResourceUsage = Resources_Schema_Logger::ClotResourceUsage;
    using ElementUsage = Resources_Schema_Logger::ElementUsage;
    using ExactMatchResultBusResourceUsage =
        Resources_Schema_Logger::ExactMatchResultBusResourceUsage;
    using ExactMatchSearchBusResourceUsage =
        Resources_Schema_Logger::ExactMatchSearchBusResourceUsage;
    using HashBitsResourceUsage = Resources_Schema_Logger::HashBitsResourceUsage;
    using HashDistResourceUsage = Resources_Schema_Logger::HashDistributionResourceUsage;
    using LogicalTableResourceUsage = Resources_Schema_Logger::LogicalTableResourceUsage;
    using MapRamResourceUsage = Resources_Schema_Logger::MapRamResourceUsage;
    using MauStageResourceUsage = Resources_Schema_Logger::MauStageResourceUsage;
    using MeterAluResourceUsage = Resources_Schema_Logger::MeterAluResourceUsage;
    using ParserResources = Resources_Schema_Logger::ParserResources;
    using PhvResourceUsage = Resources_Schema_Logger::PhvResourceUsage;
    using RamResourceUsage = Resources_Schema_Logger::RamResourceUsage;
    using ResourceUsage = Resources_Schema_Logger::ResourceUsage;
    using GatewayResourceUsage = Resources_Schema_Logger::GatewayResourceUsage;
    using StashResourceUsage = Resources_Schema_Logger::StashResourceUsage;
    using StatisticAluResourceUsage = Resources_Schema_Logger::StatisticAluResourceUsage;
    using TcamResourceUsage = Resources_Schema_Logger::TcamResourceUsage;
    using TindResultBusResourceUsage = Resources_Schema_Logger::TindResultBusResourceUsage;
    using VliwResourceUsage = Resources_Schema_Logger::VliwResourceUsage;
    using XbarResourceUsage = Resources_Schema_Logger::XbarResourceUsage;

 protected:
    const ClotInfo &clotInfo;  // ClotInfo reference is only passed to CLOT resource logger
    std::string filePath;      // path to logged file
    std::string manifestPath;  // path from manifest to logged file
    std::vector<Resources::StageResources> stageResources;  // Data for logging are collected here
    const ParserResources *parserResources = nullptr;       // Logged data for parser
    std::vector<ClotResourceUsage *> clotResources;         // Logged data for clots

    /**
     *  Prepares the object and collects data for parser and clot usage.
     */
    bool preorder(const IR::BFN::Pipe *p) override;

    /**
     *  Collects data for this particular table resource usage and
     *  stores it into stageResources.
     */
    bool preorder(const IR::MAU::Table *tbl) override;

    /**
     *  Performs actual logging - aggregates all collected data and
     *  stores them in the json file. Also updates manifest so it knows where
     *  to find that file.
     */
    void end_apply(const IR::Node *root) override;

    void collectTableUsage(cstring name, const IR::MAU::Table *table);

    void collectXbarBytesUsage(unsigned int stage, const IXBar::Use *alloc);

    void collectHashDistUsage(unsigned int stage, const Tofino::IXBar::HashDistUse &hd_use);

    void collectActionBusBytesUsage(unsigned int stage, const TableResourceAlloc *res,
                                    cstring tableName);

    void collectVliwUsage(unsigned int stage, const InstructionMemory::Use &alloc, gress_t gress,
                          cstring tableName);

    XbarResourceUsage *logXbarBytes(unsigned stageNo) const;

    HashBitsResourceUsage *logHashBits(unsigned stageNo) const;

    HashDistResourceUsage *logHashDist(unsigned stageNo) const;

    void logMemories(unsigned int stage, RamResourceUsage *ramsRes, MapRamResourceUsage *mapRamsRes,
                     GatewayResourceUsage *gatewaysRes, StashResourceUsage *stashesRes,
                     MeterAluResourceUsage *meterRes, StatisticAluResourceUsage *statisticsRes,
                     TcamResourceUsage *tcamsRes) const;

    LogicalTableResourceUsage *logLogicalTables(int stageNo) const;

    ActionDataResourceUsage *logActionBusBytes(unsigned int stageNo) const;

    void logActionSlots(MauStageResourceUsage *msru) const;

    VliwResourceUsage *logVliw(unsigned int stageNo) const;

    ExactMatchSearchBusResourceUsage *logExactMemSearchBuses(unsigned int stageNo) const;

    ExactMatchResultBusResourceUsage *logExactMemResultBuses(unsigned int stageNo) const;

    TindResultBusResourceUsage *logTindResultBuses(unsigned int stageNo) const;

    MauStageResourceUsage *logStage(int stageNo);

 public:
    // Only need this for public interface, rest is done in preorder and end_apply
    ResourcesLogging(const ClotInfo &clotInfo, const std::string &filename,
                     const std::string &outdir)
        : clotInfo(clotInfo), filePath(filename) {
        manifestPath = filename.substr(outdir.size() + 1);
    }
};

}  // namespace BFN

#endif /* _BACKENDS_TOFINO_BF_P4C_LOGGING_RESOURCES_H_ */