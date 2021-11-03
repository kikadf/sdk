/**
 * (c) 2020 by Mega Limited, Wellsford, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#pragma once

#include <mega/db.h>

#include "NotImplemented.h"

namespace mt {

class DefaultedDbTable: public mega::DbTable, public mega::DBTableNodes
{
public:
    using mega::DbTable::DbTable;
    DefaultedDbTable(mega::PrnGen& gen)
        : DbTable(gen, false)
    {
    }
    void rewind() override
    {
        //throw NotImplemented{__func__};
    }
    bool next(uint32_t*, std::string*) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool get(uint32_t, std::string*) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool getNode(mega::NodeHandle, mega::NodeSerialized&) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool getNodes(std::vector<mega::NodeSerialized>&) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool getNodesByFingerprint(const mega::FileFingerprint&, std::map<mega::NodeHandle, mega::NodeSerialized>&) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool getNodesByOrigFingerprint(const std::string& , std::map<mega::NodeHandle, mega::NodeSerialized>&) override
    {
        return false;
    }
    bool getNodeByFingerprint(const mega::FileFingerprint&, mega::NodeSerialized&, mega::NodeHandle& nodeHandle) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool getRootNodes(std::map<mega::NodeHandle, mega::NodeSerialized>& nodes) override
    {
        return false;
        //throw NotImplemented(__func__);
    }
    bool getNodesWithSharesOrLink(std::map<mega::NodeHandle, mega::NodeSerialized>& nodes, ShareType_t) override
    {
        return false;
        //throw NotImplemented(__func__);
    }
    bool getChildrenFromNode(mega::NodeHandle, std::map<mega::NodeHandle, mega::NodeSerialized>&) override
    {
        return false;
        //throw NotImplemented(__func__);
    }
    bool getChildrenHandlesFromNode(mega::NodeHandle, std::vector<mega::NodeHandle>&) override
    {
        return false;
        //throw NotImplemented(__func__);
    }
    bool getNodesByName(const std::string&, std::map<mega::NodeHandle, mega::NodeSerialized>&) override
    {
        return false;
        //throw NotImplemented(__func__);
    }
    bool getFavouritesNodeHandles(mega::NodeHandle, uint32_t, std::vector<mega::NodeHandle>&) override
    {
        return false;
    }
    mega::NodeCounter getNodeCounter(mega::NodeHandle, bool) override
    {
        mega::NodeCounter nc;
        return nc;
        //throw NotImplemented(__func__);
    }
    int getNumberOfChildrenFromNode(mega::NodeHandle) override
    {
        return 0;
        //throw NotImplemented(__func__);
    }
    bool isNodesOnDemandDb() override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    mega::NodeHandle getFirstAncestor(mega::NodeHandle h) override
    {
        return h;
        //throw NotImplemented{__func__};
    }
    bool isNodeInDB(mega::NodeHandle) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool isAncestor(mega::NodeHandle, mega::NodeHandle) override
    {
        return false;
    }
    bool isFileNode(mega::NodeHandle) override
    {
        return false;
    }
    uint64_t getNumberOfNodes() override
    {
        return false;
    }
    bool put(uint32_t, char*, unsigned) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool put(mega::Node *) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool del(uint32_t) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool del(mega::NodeHandle) override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    bool removeNodes() override
    {
        return false;
        //throw NotImplemented{__func__};
    }
    void truncate() override
    {
        //throw NotImplemented{__func__};
    }
    void begin() override
    {
        //throw NotImplemented{__func__};
    }
    void commit() override
    {
        //throw NotImplemented{__func__};
    }
    void abort() override
    {
        //throw NotImplemented{__func__};
    }
    void remove() override
    {
        //throw NotImplemented{__func__};
    }
    bool inTransaction() const override
    {
        return false;
    }
};

} // mt
