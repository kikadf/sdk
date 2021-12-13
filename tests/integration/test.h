#pragma once
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <stdio.h>
#include <map>
#include <future>
#include <fstream>
#include <atomic>
#include <random>

#include "gtest/gtest.h"

#include <mega.h>
#include <megaapi_impl.h>
#include "stdfs.h"

using namespace ::mega;
using namespace ::std;


extern string_vector envVarAccount;
extern string_vector envVarPass;

std::string logTime();
void WaitMillisec(unsigned n);

class LogStream
{
public:
    LogStream()
      : mBuffer()
    {
    }

    LogStream(LogStream&& other) noexcept
      : mBuffer(std::move(other.mBuffer))
    {
    }

    ~LogStream();

    template<typename T>
    LogStream& operator<<(const T* value)
    {
        mBuffer << value;
        return *this;
    }

    template<typename T, typename = typename std::enable_if<std::is_scalar<T>::value>::type>
    LogStream& operator<<(const T value)
    {
        mBuffer << value;
        return *this;
    }

    template<typename T, typename = typename std::enable_if<!std::is_scalar<T>::value>::type>
    LogStream& operator<<(const T& value)
    {
        mBuffer << value;
        return *this;
    }

private:
    std::ostringstream mBuffer;
}; // LogStream

extern std::string USER_AGENT;
extern bool gRunningInCI;
extern bool gTestingInvalidArgs;
extern bool gResumeSessions;
extern bool gScanOnly;
extern int gFseventsFd;

extern bool WaitFor(std::function<bool()>&& f, unsigned millisec);

LogStream out();

enum { THREADS_PER_MEGACLIENT = 3 };

class TestingWithLogErrorAllowanceGuard
{
public:
    TestingWithLogErrorAllowanceGuard()
    {
        gTestingInvalidArgs = true;
    }
    ~TestingWithLogErrorAllowanceGuard()
    {
        gTestingInvalidArgs = false;
    }
};

class TestFS
{
public:
    // these getters should return std::filesystem::path type, when C++17 will become mandatory
    static fs::path GetTestBaseFolder();
    static fs::path GetTestFolder();
    static fs::path GetTrashFolder();

    void DeleteTestFolder() { DeleteFolder(GetTestFolder()); }
    void DeleteTrashFolder() { DeleteFolder(GetTrashFolder()); }

    ~TestFS();

private:
    void DeleteFolder(fs::path folder);

    std::vector<std::thread> m_cleaners;
};

void moveToTrash(const fs::path& p);
fs::path makeNewTestRoot();
fs::path makeReusableClientFolder(const string& subfolder);

std::unique_ptr<::mega::FileSystemAccess> makeFsAccess();

template<typename T>
using shared_promise = std::shared_ptr<promise<T>>;

using PromiseBoolSP     = shared_promise<bool>;
using PromiseHandleSP   = shared_promise<handle>;
using PromiseStringSP   = shared_promise<string>;
using PromiseUnsignedSP = shared_promise<unsigned>;

struct Model
{
    // records what we think the tree should look like after sync so we can confirm it

    struct ModelNode
    {
        enum nodetype { file, folder };
        nodetype type = folder;
        string mCloudName;
        string mFsName;
        string name;
        string content;
        vector<unique_ptr<ModelNode>> kids;
        ModelNode* parent = nullptr;
        bool changed = false;
        bool fsOnly = false;

        ModelNode() = default;

        ModelNode(const ModelNode& other);
        ModelNode& fsName(const string& name);
        const string& fsName() const;
        ModelNode& cloudName(const string& name);
        const string& cloudName() const;
        void generate(const fs::path& path, bool force);
        string path();
        string fsPath();
        ModelNode* addkid();
        ModelNode* addkid(unique_ptr<ModelNode>&& p);
        bool typematchesnodetype(nodetype_t nodetype) const;
        void print(string prefix="");
        std::unique_ptr<ModelNode> clone();
    };

    Model();
    Model(const Model& other);
    Model& operator=(const Model& rhs);
    ModelNode* addfile(const string& path, const string& content);
    ModelNode* addfile(const string& path);
    ModelNode* addfolder(const string& path);
    ModelNode* addnode(const string& path, ModelNode::nodetype type);
    ModelNode* copynode(const string& src, const string& dst);
    unique_ptr<ModelNode> makeModelSubfolder(const string& utf8Name);
    unique_ptr<ModelNode> makeModelSubfile(const string& utf8Name, string content = {});
    unique_ptr<ModelNode> buildModelSubdirs(const string& prefix, int n, int recurselevel, int filesperdir);
    ModelNode* childnodebyname(ModelNode* n, const std::string& s);
    ModelNode* findnode(string path, ModelNode* startnode = nullptr);
    unique_ptr<ModelNode> removenode(const string& path);
    bool movenode(const string& sourcepath, const string& destpath);
    bool movetosynctrash(const string& path, const string& syncrootpath);
    void ensureLocalDebrisTmpLock(const string& syncrootpath);
    bool removesynctrash(const string& syncrootpath, const string& subpath = "");
    void emulate_rename(std::string nodepath, std::string newname);
    void emulate_move(std::string nodepath, std::string newparentpath);
    void emulate_copy(std::string nodepath, std::string newparentpath);
    void emulate_rename_copy(std::string nodepath, std::string newparentpath, std::string newname);
    void emulate_delete(std::string nodepath);
    void generate(const fs::path& path, bool force = false);
    void swap(Model& other);
    unique_ptr<ModelNode> root;
};


struct StandardClient : public ::mega::MegaApp
{
    WAIT_CLASS waiter;
#ifdef GFX_CLASS
    GFX_CLASS gfx;
#endif

    string client_dbaccess_path;
    std::unique_ptr<HttpIO> httpio;
    std::recursive_mutex clientMutex;
    MegaClient client;
    std::atomic<bool> clientthreadexit{false};
    bool fatalerror = false;
    string clientname;
    std::function<void()> nextfunctionMC;
    std::function<void()> nextfunctionSC;
    std::condition_variable functionDone;
    std::mutex functionDoneMutex;
    std::string salt;
    std::set<fs::path> localFSFilesThatMayDiffer;

    fs::path fsBasePath;

    handle basefolderhandle = UNDEF;

    enum resultprocenum { PRELOGIN, LOGIN, FETCHNODES, PUTNODES, UNLINK, CATCHUP,
        COMPLETION };  // use COMPLETION when we use a completion function, rather than trying to match tags on callbacks

    struct ResultProc
    {
        StandardClient& client;
        ResultProc(StandardClient& c) : client(c) {}

        struct id_callback
        {
            int request_tag = 0;
            handle h = UNDEF;
            std::function<bool(error)> f;
            id_callback(std::function<bool(error)> cf, int tag, handle ch) : request_tag(tag), h(ch), f(cf) {}
        };

        recursive_mutex mtx;  // recursive because sometimes we need to set up new operations during a completion callback
        map<resultprocenum, deque<id_callback>> m;

        void prepresult(resultprocenum rpe, int tag, std::function<void()>&& requestfunc, std::function<bool(error)>&& f, handle h = UNDEF);
        void processresult(resultprocenum rpe, error e, handle h = UNDEF);
    } resultproc;

    // thread as last member so everything else is initialised before we start it
    std::thread clientthread;

    string ensureDir(const fs::path& p);
    StandardClient(const fs::path& basepath, const string& name, const fs::path& workingFolder = fs::path());
    ~StandardClient();
    void localLogout();

    static mutex om;
    bool logcb = false;
    chrono::steady_clock::time_point lastcb = std::chrono::steady_clock::now();

    string lp(LocalNode* ln);

    void onCallback();

    std::function<void(const SyncConfig&, bool, bool)> onAutoResumeResult;

    void sync_auto_resume_result(const SyncConfig& config, bool attempted, bool hadAnError) override;

    bool received_syncs_restored = false;
    void syncs_restored(SyncError syncError) override;

    bool received_node_actionpackets = false;
    std::condition_variable nodes_updated_cv;

    void nodes_updated(Node** nodes, int numNodes) override;
    bool waitForNodesUpdated(unsigned numSeconds);
    void syncupdate_stateconfig(const SyncConfig& config) override;
    void syncupdate_scanning(bool b) override;

    std::atomic<bool> mStallDetected{false};

    void syncupdate_stalled(bool b) override;
    void file_added(File* file) override;
    void file_complete(File* file) override;
    void syncupdate_filter_error(const SyncConfig& config) override;

    void syncupdate_local_lockretry(bool b) override;

    std::atomic<unsigned> transfersAdded{0}, transfersRemoved{0}, transfersPrepared{0}, transfersFailed{0}, transfersUpdated{0}, transfersComplete{0};

    void transfer_added(Transfer*) override { onCallback(); ++transfersAdded; }
    void transfer_removed(Transfer*) override { onCallback(); ++transfersRemoved; }
    void transfer_prepare(Transfer*) override { onCallback(); ++transfersPrepared; }
    void transfer_failed(Transfer*,  const Error&, dstime = 0) override { onCallback(); ++transfersFailed; }
    void transfer_update(Transfer*) override { onCallback(); ++transfersUpdated; }

    std::function<void(Transfer*)> onTransferCompleted;

    void transfer_complete(Transfer* transfer) override
    {
        onCallback();

        if (onTransferCompleted)
            onTransferCompleted(transfer);

        ++transfersComplete;
    }

    void notify_retry(dstime t, retryreason_t r) override;
    void request_error(error e) override;
    void request_response_progress(m_off_t a, m_off_t b) override;
    void threadloop();

    static bool debugging;  // turn this on to prevent the main thread timing out when stepping in the MegaClient

    template <class PROMISE_VALUE>
    future<PROMISE_VALUE> thread_do(std::function<void(MegaClient&, shared_promise<PROMISE_VALUE>)> f)
    {
        unique_lock<mutex> guard(functionDoneMutex);
        std::shared_ptr<promise<PROMISE_VALUE>> promiseSP(new promise<PROMISE_VALUE>());
        nextfunctionMC = [this, promiseSP, f](){ f(this->client, promiseSP); };
        waiter.notify();
        while (!functionDone.wait_until(guard, chrono::steady_clock::now() + chrono::seconds(600), [this]() { return !nextfunctionMC; }))
        {
            if (!debugging)
            {
                promiseSP->set_value(PROMISE_VALUE());
                break;
            }
        }
        return promiseSP->get_future();
    }

    template <class PROMISE_VALUE>
    future<PROMISE_VALUE> thread_do(std::function<void(StandardClient&, shared_promise<PROMISE_VALUE>)> f)
    {
        unique_lock<mutex> guard(functionDoneMutex);
        std::shared_ptr<promise<PROMISE_VALUE>> promiseSP(new promise<PROMISE_VALUE>());
        nextfunctionMC = [this, promiseSP, f]() { f(*this, promiseSP); };
        waiter.notify();
        while (!functionDone.wait_until(guard, chrono::steady_clock::now() + chrono::seconds(600), [this]() { return !nextfunctionSC; }))
        {
            if (!debugging)
            {
                promiseSP->set_value(PROMISE_VALUE());
                break;
            }
        }
        return promiseSP->get_future();
    }

    void preloginFromEnv(const string& userenv, PromiseBoolSP pb);
    void loginFromEnv(const string& userenv, const string& pwdenv, PromiseBoolSP pb);
    void loginFromSession(const string& session, PromiseBoolSP pb);
    bool cloudCopyTreeAs(Node* from, Node* to, string name);

    class BasicPutNodesCompletion
    {
    public:
        BasicPutNodesCompletion(std::function<void(const Error&)>&& callable)
            : mCallable(std::move(callable))
        {
        }

        void operator()(const Error& e, targettype_t, vector<NewNode>&, bool)
        {
            mCallable(e);
        }

    private:
        std::function<void(const Error&)> mCallable;
    }; // BasicPutNodesCompletion

    void cloudCopyTreeAs(Node* n1, Node* n2, std::string newname, PromiseBoolSP pb);
    void putnodes(NodeHandle parentHandle, std::vector<NewNode>&& nodes, PromiseBoolSP pb);
    bool putnodes(NodeHandle parentHandle, std::vector<NewNode>&& nodes);
    void putnodes(const string& parentPath, std::vector<NewNode>&& nodes, PromiseBoolSP result);
    bool putnodes(const string &parentPath, std::vector<NewNode>&& nodes);
    void uploadFolderTree_recurse(handle parent, handle& h, const fs::path& p, vector<NewNode>& newnodes);
    void uploadFolderTree(fs::path p, Node* n2, PromiseBoolSP pb);

    // Necessary to make sure we release the file once we're done with it.
    struct FileGet : public File {
        void completed(Transfer* t, putsource_t source) override
        {
            File::completed(t, source);
            result->set_value(true);
            delete this;
        }

        void terminated() override
        {
            result->set_value(false);
            delete this;
        }

        PromiseBoolSP result;
    }; // FileGet

    void downloadFile(const Node& node, const fs::path& destination, PromiseBoolSP result);
    bool downloadFile(const Node& node, const fs::path& destination);

    struct FilePut : public File {
        void completed(Transfer* t, putsource_t source) override
        {
            File::completed(t, source);
            delete this;
        }

        void terminated() override
        {
            delete this;
        }
    }; // FilePut

    bool uploadFolderTree(fs::path p, Node* n2);
    void uploadFile(const fs::path& path, const string& name, Node* parent, bool fixNameConflicts, DBTableTransactionCommitter& committer);
    void uploadFile(const fs::path& path, const string& name, Node* parent, bool fixNameConflicts, PromiseBoolSP pb);
    bool uploadFile(const fs::path& path, const string& name, Node* parent, bool fixNameConflicts = true, int timeoutSeconds = 30);
    bool uploadFile(const fs::path& path, const string& name, string parentPath, bool fixNameConflicts = true, int timeoutSeconds = 30);
    bool uploadFile(const fs::path& path, Node* parent, bool fixNameConflicts = true, int timeoutSeconds = 30);
    bool uploadFile(const fs::path& path, const string& parentPath, bool fixNameConflicts = true, int timeoutSeconds = 30);
    void uploadFilesInTree_recurse(Node* target, const fs::path& p, std::atomic<int>& inprogress, bool fixNameConflicts, DBTableTransactionCommitter& committer);
    bool uploadFilesInTree(fs::path p, Node* n2, bool fixNameConflicts = true);
    void uploadFilesInTree(fs::path p, Node* n2, std::atomic<int>& inprogress, bool fixNameConflicts, PromiseBoolSP pb);

    class TreeProcPrintTree : public TreeProc
    {
    public:
        void proc(MegaClient* client, Node* n) override
        {
            //out() << "fetchnodes tree: " << n->displaypath();;
        }
    };

    // mark node as removed and notify

    std::function<void (StandardClient& mc, PromiseBoolSP pb)> onFetchNodes;

    void fetchnodes(bool noCache, PromiseBoolSP pb);
    bool fetchnodes(bool noCache = false);
    NewNode makeSubfolder(const string& utf8Name);
    void catchup(PromiseBoolSP pb);

    unsigned deleteTestBaseFolder(bool mayNeedDeleting);
    void deleteTestBaseFolder(bool mayNeedDeleting, bool deleted, PromiseUnsignedSP result);

    void ensureTestBaseFolder(bool mayneedmaking, PromiseBoolSP pb);
    NewNode* buildSubdirs(list<NewNode>& nodes, const string& prefix, int n, int recurselevel);
    bool makeCloudSubdirs(const string& prefix, int depth, int fanout);
    void makeCloudSubdirs(const string& prefix, int depth, int fanout, PromiseBoolSP pb, const string& atpath = "");

    struct SyncInfo
    {
        NodeHandle h;
        fs::path localpath;
        string remotepath;
    };

    SyncConfig syncConfigByBackupID(handle backupID) const;
    bool syncSet(handle backupId, SyncInfo& info) const;
    SyncInfo syncSet(handle backupId);
    SyncInfo syncSet(handle backupId) const;
    Node* getcloudrootnode();
    Node* gettestbasenode();
    Node* getcloudrubbishnode();
    Node* getsyncdebrisnode();
    Node* drillchildnodebyname(Node* n, const string& path);
    vector<Node*> drillchildnodesbyname(Node* n, const string& path);

    bool backupAdd_inthread(const string& drivePath,
        string sourcePath,
        const string& targetPath,
        std::function<void(error, SyncError, handle)> completion,
        const string& logname);
    handle backupAdd_mainthread(const string& drivePath,
        const string& sourcePath,
        const string& targetPath,
        const string& logname);
    bool setupSync_inthread(const string& subfoldername, const fs::path& localpath, const bool isBackup,
        std::function<void(error, SyncError, handle)> addSyncCompletion, const string& logname);
    void importSyncConfigs(string configs, PromiseBoolSP result);
    bool importSyncConfigs(string configs);
    string exportSyncConfigs();
    bool delSync_inthread(handle backupId, bool keepCache);

    struct CloudNameLess
    {
        bool operator()(const string& lhs, const string& rhs) const
        {
            return compare(lhs, rhs) < 0;
        }

        static int compare(const string& lhs, const string& rhs)
        {
            return compareUtf(lhs, false, rhs, false, false);
        }

        static bool equal(const string& lhs, const string& rhs)
        {
            return compare(lhs, rhs) == 0;
        }
    }; // CloudNameLess

    bool recursiveConfirm(Model::ModelNode* mn, Node* n, int& descendants, const string& identifier, int depth, bool& firstreported, bool expectFail, bool skipIgnoreFile);

    bool localNodesMustHaveNodes = true;

    auto equal_range_utf8EscapingCompare(multimap<string, LocalNode*, CloudNameLess>& ns, const string& cmpValue, bool unescapeValue, bool unescapeMap, bool caseInsensitive) -> std::pair<multimap<string, LocalNode*>::iterator, multimap<string, LocalNode*>::iterator>;
    bool recursiveConfirm(Model::ModelNode* mn, LocalNode* n, int& descendants, const string& identifier, int depth, bool& firstreported, bool expectFail, bool skipIgnoreFile);
    bool recursiveConfirm(Model::ModelNode* mn, fs::path p, int& descendants, const string& identifier, int depth, bool ignoreDebris, bool& firstreported, bool expectFail, bool skipIgnoreFile);
    Sync* syncByBackupId(handle backupId);
    bool setSyncPausedByBackupId(handle id, bool pause);
    void enableSyncByBackupId(handle id, PromiseBoolSP result, const string& logname);
    bool enableSyncByBackupId(handle id, const string& logname);
    void backupIdForSyncPath(const fs::path& path, PromiseHandleSP result);
    handle backupIdForSyncPath(fs::path path);

    enum Confirm
    {
        CONFIRM_LOCALFS = 0x01,
        CONFIRM_LOCALNODE = 0x02,
        CONFIRM_LOCAL = CONFIRM_LOCALFS | CONFIRM_LOCALNODE,
        CONFIRM_REMOTE = 0x04,
        CONFIRM_ALL = CONFIRM_LOCAL | CONFIRM_REMOTE,
    };

    bool confirmModel_mainthread(handle id, Model::ModelNode* mRoot, Node* rRoot, bool expectFail, bool skipIgnoreFile);
    bool confirmModel_mainthread(handle id, Model::ModelNode* mRoot, LocalNode* lRoot, bool expectFail, bool skipIgnoreFile);
    bool confirmModel_mainthread(handle id, Model::ModelNode* mRoot, fs::path lRoot, bool ignoreDebris, bool expectFail, bool skipIgnoreFile);
    bool confirmModel(handle id, Model::ModelNode* mRoot, Node* rRoot, bool expectFail, bool skipIgnoreFile);
    bool confirmModel(handle id, Model::ModelNode* mRoot, LocalNode* lRoot, bool expectFail, bool skipIgnoreFile);
    bool confirmModel(handle id, Model::ModelNode* mRoot, fs::path lRoot, bool ignoreDebris, bool expectFail, bool skipIgnoreFile);
    bool confirmModel(handle backupId, Model::ModelNode* mnode, const int confirm, const bool ignoreDebris, bool expectFail, bool skipIgnoreFile);
    void prelogin_result(int, string*, string* salt, error e) override;
    void login_result(error e) override;
    void fetchnodes_result(const Error& e) override;
    bool setattr(Node* node, attr_map&& updates);
    void setattr(Node* node, attr_map&& updates, PromiseBoolSP result);
    bool rename(const string& path, const string& newName);
    void unlink_result(handle h, error e) override;

    handle lastPutnodesResultFirstHandle = UNDEF;

    void putnodes_result(const Error& e, targettype_t tt, vector<NewNode>& nn, bool targetOverride) override;
    void catchup_result() override;

    void disableSync(handle id, SyncError error, bool enabled, PromiseBoolSP result);
    bool disableSync(handle id, SyncError error, bool enabled);

    template<typename ResultType, typename Callable>
    ResultType withWait(Callable&& callable, ResultType&& defaultValue = ResultType())
    {
        using std::future_status;
        using std::shared_ptr;

        using PromiseType = promise<ResultType>;
        using PointerType = shared_ptr<PromiseType>;

        auto promise = PointerType(new PromiseType());
        auto future = promise->get_future();

        callable(std::move(promise));

        auto status = future.wait_for(std::chrono::seconds(20));

        if (status == future_status::ready)
        {
            return future.get();
        }

        LOG_warn << "Timed out in withWait";

        return std::move(defaultValue);
    }

    void deleteremote(string path, bool fromroot, PromiseBoolSP pb);
    bool deleteremote(string path, bool fromroot = false);
    bool deleteremote(Node* node);
    void deleteremote(Node* node, PromiseBoolSP result);
    bool deleteremotedebris();
    void deleteremotedebris(PromiseBoolSP result);
    bool deleteremotenode(Node* node);
    void deleteremotenodes(vector<Node*> ns, PromiseBoolSP pb);
    bool movenode(string path, string newParentPath, string newName = "");
    void movenode(string path, string newname, string newparentpath, PromiseBoolSP pb);
    void movenode(handle h1, handle h2, PromiseBoolSP pb);
    void movenodetotrash(string path, PromiseBoolSP pb);
    void exportnode(Node* n, int del, m_time_t expiry, bool writable, promise<Error>& pb);
    void getpubliclink(Node* n, int del, m_time_t expiry, bool writable, promise<Error>& pb);
    void waitonsyncs(chrono::seconds d = chrono::seconds(2));
    bool conflictsDetected(list<NameConflict>& conflicts);
    bool login_reset(bool noCache = false);
    bool login_reset(const string& user, const string& pw, bool noCache = false, bool resetBaseCloudFolder = true);
    bool resetBaseFolderMulticlient(StandardClient* c2 = nullptr, StandardClient* c3 = nullptr, StandardClient* c4 = nullptr);
    void cleanupForTestReuse();
    bool login_reset_makeremotenodes(const string& prefix, int depth = 0, int fanout = 0, bool noCache = false);
    bool login_reset_makeremotenodes(const string& user, const string& pw, const string& prefix, int depth, int fanout, bool noCache = false);
    void ensureSyncUserAttributes(PromiseBoolSP result);
    bool ensureSyncUserAttributes();
    void copySyncConfig(SyncConfig config, PromiseHandleSP result);
    handle copySyncConfig(const SyncConfig& config);
    bool login(const string& user, const string& pw);
    bool login_fetchnodes(const string& user, const string& pw, bool makeBaseFolder = false, bool noCache = false);
    bool login_fetchnodes(const string& session);
    handle setupSync_mainthread(const std::string& localsyncrootfolder, const std::string& remotesyncrootfolder, bool isBackup = false, bool uploadIgnoreFirst = true);
    bool delSync_mainthread(handle backupId, bool keepCache = false);
    bool confirmModel_mainthread(Model::ModelNode* mnode, handle backupId, bool ignoreDebris = false, int confirm = CONFIRM_ALL, bool expectFail = false, bool skipIgnoreFile = true);
    bool match(handle id, const Model::ModelNode* source);
    void match(handle id, const Model::ModelNode* source, PromiseBoolSP result);
    bool waitFor(std::function<bool(StandardClient&)>&& predicate, const std::chrono::seconds &timeout);
    bool match(const Node& destination, const Model::ModelNode& source) const;
    bool backupOpenDrive(const fs::path& drivePath);
    void wouldBeEscapedOnDownload(fs::path root, string remoteName, PromiseBoolSP result);
    bool wouldBeEscapedOnDownload(fs::path root, string remoteName);
    size_t triggerFullScan(handle backupID);

    function<void(File&)> mOnFileAdded;
    function<void(File&)> mOnFileComplete;
    function<void(const SyncConfig&)> mOnFilterError;
    function<void(bool)>  mOnStall;

};


struct StandardClientInUseEntry
{
    bool inUse = false;
    shared_ptr<StandardClient> ptr;
    string name;

    StandardClientInUseEntry(bool iu, shared_ptr<StandardClient> sp, string n)
    : inUse(iu)
    , ptr(sp)
    , name(n)
    {}
};


class StandardClientInUse
{
    list<StandardClientInUseEntry>::iterator entry;

public:

    StandardClientInUse(list<StandardClientInUseEntry>::iterator i)
    : entry(i)
    {
        assert(!entry->inUse);
        entry->inUse = true;
    }

    ~StandardClientInUse()
    {
        entry->ptr->cleanupForTestReuse();
        entry->inUse = false;
    }

    StandardClient* operator->()
    {
        return entry->ptr.get();
    }

    operator StandardClient*()
    {
        return entry->ptr.get();
    }

    operator StandardClient&()
    {
        return *entry->ptr;
    }

};

class ClientManager
{
    // reuse the same client for subsequent tests, to save all the time of logging in, fetchnodes, etc.

    map<int, list<StandardClientInUseEntry>> clients;

public:

    StandardClientInUse getCleanStandardClient(int loginIndex, fs::path workingFolder);

    void shutdown();
};

extern ClientManager g_clientManager;


