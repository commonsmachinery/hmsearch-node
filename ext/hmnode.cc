/* HmSearch hash lookup - Node.js interface
 *
 * Copyright 2014 Commons Machinery http://commonsmachinery.se/
 * Distributed under an MIT license, please see LICENSE in the top dir.
 */

#include <node.h>
#include <nan.h>

#include <hmsearch.h>

using namespace v8;

/*
 * Wrap HmSearch in a javascript object
 */

class HmObject : public node::ObjectWrap {
public:
    static void init();

    static Local<Value> create_with_db(HmSearch *db);

private:
    HmObject()
        : _db(NULL)
        , _db_users(0)
        {
            uv_mutex_init(&_lock);
            uv_cond_init(&_cond);
        }

    ~HmObject() {
        if (_db) {
            delete _db;
            _db = NULL;
        }
    }

    static NAN_METHOD(New);
    static HmObject* unwrap(Handle<Object> handle);

    static Persistent<Function> constructor;
    static Persistent<FunctionTemplate> prototype;

    static NAN_METHOD(insert_sync);
    static NAN_METHOD(lookup_sync);
    static NAN_METHOD(close_sync);

    // Since we may have worker threads active, we must protect the
    // database object from being closed while any insert or lookup
    // operation is in progress.

    // Get the database, increasing the user count
    HmSearch* get_db();

    // Release the database, decreasing the user count
    void release_db();

    uv_mutex_t _lock;
    uv_cond_t _cond;
    HmSearch* _db;
    int _db_users;
};

Persistent<Function> HmObject::constructor;
Persistent<FunctionTemplate> HmObject::prototype;


/*
 * Async workers
 */
class InitWorker : public NanAsyncWorker
{
public:
    InitWorker(NanCallback *callback,
               Handle<Value> path,
               unsigned hash_bits,
               unsigned max_error,
               uint64_t num_hashes
        )
        : NanAsyncWorker(callback)
        , _path(path)
        , _hash_bits(hash_bits)
        , _max_error(max_error)
        , _num_hashes(num_hashes)
        { }

    void Execute() {
        std::string error_msg;
        if (!HmSearch::init(*_path, _hash_bits, _max_error, _num_hashes, &error_msg)) {
            SetErrorMessage(error_msg.c_str());
        }
    }

private:
    NanUtf8String _path;
    unsigned _hash_bits;
    unsigned _max_error;
    uint64_t _num_hashes;
};


class OpenWorker : public NanAsyncWorker
{
public:
    OpenWorker(NanCallback *callback,
               Handle<Value> path,
               HmSearch::OpenMode mode)
        : NanAsyncWorker(callback)
        , _path(path)
        , _mode(mode)
        , _db(NULL)
        { }

    void Execute() {
        std::string error_msg;
        _db = HmSearch::open(*_path, _mode, &error_msg);
        if (!_db) {
            SetErrorMessage(error_msg.c_str());
        }
    }

    void HandleOKCallback() {
        NanScope();

        Local<Value> obj = HmObject::create_with_db(_db);
        Local<Value> argv[] = { NanNull(), obj };

        callback->Call(2, argv);
    }

private:
    NanUtf8String _path;
    HmSearch::OpenMode _mode;
    HmSearch* _db;
};


class HmObjectWorker : public NanAsyncWorker
{
public:
    HmObjectWorker(NanCallback *callback,
             Handle<Object> handle,
             HmObject* obj)
        : NanAsyncWorker(callback)
        , _obj(obj)
        {
            // Make sure the object isn't GC:d under our feet
            NanAssignPersistent(_handle, handle);
        }

    ~HmObjectWorker() {
        NanDisposePersistent(_handle);
    }

private:
    Persistent<Object> _handle;
    HmObject *_obj;
};



void HmObject::init()
{
    NanScope();

    // Prepare constructor template
    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);

    tpl->SetClassName(NanNew<String>("HmObject"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    NanSetPrototypeTemplate(tpl, "insertSync",
                            NanNew<FunctionTemplate>(insert_sync)->GetFunction());
    NanSetPrototypeTemplate(tpl, "lookupSync",
                            NanNew<FunctionTemplate>(lookup_sync)->GetFunction());
    NanSetPrototypeTemplate(tpl, "closeSync",
                            NanNew<FunctionTemplate>(close_sync)->GetFunction());

    NanAssignPersistent(prototype, tpl);
    NanAssignPersistent(constructor, tpl->GetFunction());
}


Local<Value> HmObject::create_with_db(HmSearch *db)
{
    NanEscapableScope();

    Local<Value> jsobj = constructor->NewInstance();
    HmObject* obj = ObjectWrap::Unwrap<HmObject>(jsobj->ToObject());
    obj->_db = db;

    return NanEscapeScope(jsobj);
}


HmObject* HmObject::unwrap(Handle<Object> handle)
{
    // Sanity check the object before we accept it as one of our own
    // wrapped objects

    if (NanHasInstance(prototype, handle)) {
        return ObjectWrap::Unwrap<HmObject>(handle);
    }

    NanThrowTypeError("<this> is not a hmsearch object");
    return NULL;
}


HmSearch* HmObject::get_db()
{
    HmSearch *db = NULL;

    uv_mutex_lock(&_lock);
    if (_db) {
        db = _db;
        ++_db_users;
    }
    uv_mutex_unlock(&_lock);

    return db;
}

void HmObject::release_db()
{
    uv_mutex_lock(&_lock);
    if (--_db_users <= 0) {
        // Tell pending close operation that it can proceed
        uv_cond_broadcast(&_cond);
    }
    uv_mutex_unlock(&_lock);
}


NAN_METHOD(HmObject::New)
{
    NanScope();

    if (args.IsConstructCall()) {
        // Invoked as constructor: `new HmObject(...)`
        HmObject* obj = new HmObject();
        obj->Wrap(args.This());
        NanReturnValue(args.This());
    }
    else {
        // Invoked as plain function `HmObject(...)`, turn into construct call.
        NanReturnValue(constructor->NewInstance());
    }
}


NAN_METHOD(HmObject::insert_sync)
{
    NanScope();

    HmObject* obj = unwrap(args.This());

    if (!obj) {
        NanReturnUndefined();
    }

    if (args.Length() != 1) {
        NanThrowTypeError("Wrong number of arguments");
        NanReturnUndefined();
    }

    if (!args[0]->IsString()) {
        NanThrowTypeError("Wrong arguments");
        NanReturnUndefined();
    }

    if (!obj->_db) {
        NanThrowError("database is closed");
        NanReturnUndefined();
    }

    NanAsciiString hash(args[0]);
    if (obj->_db->insert(HmSearch::parse_hexhash(*hash))) {
        NanReturnUndefined();
    }
    else {
        NanThrowError(obj->_db->get_last_error());
        NanReturnUndefined();
    }
}


NAN_METHOD(HmObject::lookup_sync)
{
    NanScope();

    HmObject* obj = unwrap(args.This());

    if (!obj) {
        NanReturnUndefined();
    }

    if (args.Length() < 1 || args.Length() > 2) {
        NanThrowTypeError("Wrong number of arguments");
        NanReturnUndefined();
    }

    if (!args[0]->IsString() ||
        (args.Length() > 1 && !args[1]->IsNumber())) {
        NanThrowTypeError("Wrong arguments");
        NanReturnUndefined();
    }

    if (!obj->_db) {
        NanThrowError("database is closed");
        NanReturnUndefined();
    }

    NanAsciiString hash(args[0]);
    int max_error = args.Length() > 1 ? args[1]->IntegerValue() : -1;

    HmSearch::LookupResultList matches;
    if (obj->_db->lookup(HmSearch::parse_hexhash(*hash), matches, max_error)) {
        size_t elements = matches.size();
        Local<Array> a = NanNew<Array>(elements);

        if (a.IsEmpty()) {
            // error creating the array
            NanReturnUndefined();
        }

        int i = 0;
        for (HmSearch::LookupResultList::const_iterator res = matches.begin();
             res != matches.end();
             ++res, ++i) {
            Local<Object> obj = NanNew<Object>();
            std::string hexhash = HmSearch::format_hexhash(res->hash);
            obj->Set(NanNew<String>("hash"), NanNew<String>(hexhash.c_str()));
            obj->Set(NanNew<String>("distance"), NanNew<Integer>(res->distance));
            a->Set(i, obj);
        }

        NanReturnValue(a);
    }
    else {
        NanThrowError(obj->_db->get_last_error());
        NanReturnUndefined();
    }
}


NAN_METHOD(HmObject::close_sync)
{
    NanScope();
    
    HmObject* obj = unwrap(args.This());

    if (!obj) {
        NanReturnUndefined();
    }

    if (args.Length() != 0) {
        NanThrowTypeError("Wrong number of arguments");
        NanReturnUndefined();
    }

    if (obj->_db) {
        if (!obj->_db->close()) {
            NanThrowError(obj->_db->get_last_error());
        }

        delete obj->_db;
        obj->_db = NULL;
    }

    NanReturnUndefined();
}


/*
 * Module interface
 */

static NAN_METHOD(init_sync)
{
    NanScope();

    if (args.Length() != 4) {
        NanThrowTypeError("Wrong number of arguments");
        NanReturnUndefined();
    }

    if (!args[0]->IsString() || !args[1]->IsNumber() || !args[2]->IsNumber() || !args[3]->IsNumber()) {
        NanThrowTypeError("Wrong arguments");
        NanReturnUndefined();
    }

    NanUtf8String path(args[0]);
    unsigned hash_bits = args[1]->IntegerValue();
    unsigned max_error = args[2]->IntegerValue();
    uint64_t num_hashes = args[3]->NumberValue();
    std::string error_msg;

    if (!HmSearch::init(*path, hash_bits, max_error, num_hashes, &error_msg)) {
        NanThrowError(error_msg.c_str());
    }

    NanReturnUndefined();
}


static NAN_METHOD(init_cb)
{
    NanScope();

    if (args.Length() != 5) {
        NanThrowTypeError("Wrong number of arguments");
        NanReturnUndefined();
    }

    if (!args[0]->IsString() || !args[1]->IsNumber() || !args[2]->IsNumber() ||
        !args[3]->IsNumber() || !args[4]->IsFunction()) {
        NanThrowTypeError("Wrong arguments");
        NanReturnUndefined();
    }

    Local<Function> callbackHandle = args[4].As<Function>();

    NanAsyncQueueWorker(
        new InitWorker(
            new NanCallback(callbackHandle),
            args[0],
            args[1]->IntegerValue(),
            args[2]->IntegerValue(),
            args[3]->NumberValue()));

    NanReturnUndefined();
}


static NAN_METHOD(open_sync)
{
    NanScope();

    if (args.Length() != 2) {
        NanThrowTypeError("Wrong number of arguments");
        NanReturnUndefined();
    }

    if (!args[0]->IsString() || !args[1]->IsNumber()) {
        NanThrowTypeError("Wrong arguments");
        NanReturnUndefined();
    }

    NanUtf8String path(args[0]);
    HmSearch::OpenMode mode = HmSearch::OpenMode(args[1]->IntegerValue());
    std::string error_msg;

    HmSearch* db = HmSearch::open(*path, mode, &error_msg);

    if (db) {
        NanReturnValue(HmObject::create_with_db(db));
    }
    else {
        NanThrowError(error_msg.c_str());
        NanReturnUndefined();
    }
}


static NAN_METHOD(open_cb)
{
    NanScope();

    if (args.Length() != 3) {
        NanThrowTypeError("Wrong number of arguments");
        NanReturnUndefined();
    }

    if (!args[0]->IsString() || !args[1]->IsNumber() || !args[2]->IsFunction()) {
        NanThrowTypeError("Wrong arguments");
        NanReturnUndefined();
    }

    Local<Function> callbackHandle = args[2].As<Function>();

    NanAsyncQueueWorker(
        new OpenWorker(
            new NanCallback(callbackHandle),
            args[0],
            HmSearch::OpenMode(args[1]->IntegerValue())));

    NanReturnUndefined();
}



void module_init(Handle<Object> exports) {
    HmObject::init();

    exports->Set(NanNew<String>("READONLY"),
                 NanNew<Integer>(HmSearch::READONLY),
                 static_cast<PropertyAttribute>(ReadOnly|DontDelete));

    exports->Set(NanNew<String>("READWRITE"),
                 NanNew<Integer>(HmSearch::READWRITE),
                 static_cast<PropertyAttribute>(ReadOnly|DontDelete));

    node::SetMethod(exports, "init", init_cb);
    node::SetMethod(exports, "initSync", init_sync);
    node::SetMethod(exports, "open", open_cb);
    node::SetMethod(exports, "openSync", open_sync);
}

NODE_MODULE(hmsearch, module_init)


/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/
