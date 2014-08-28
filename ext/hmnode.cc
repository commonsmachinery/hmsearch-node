/* HmSearch hash lookup - Node.js interface
 *
 * Copyright 2014 Commons Machinery http://commonsmachinery.se/
 * Distributed under an MIT license, please see LICENSE in the top dir.
 */

#include <node.h>
#include <v8.h>

#include <hmsearch.h>

using namespace v8;

/*
 * Wrap HmSearch in a javascript object
 */

class HmObject : public node::ObjectWrap {
public:
    static void init();

    static Handle<Value> create_with_db(HmSearch *db);

private:
    HmObject() : _db(NULL) { }
    ~HmObject() {
        if (_db) {
            delete _db;
            _db = NULL;
        }
    }

    static Handle<Value> New(const Arguments& args);
    static HmObject* unwrap(Handle<Object> handle);
    static Persistent<Function> constructor;
    static Persistent<Value> prototype;

    static Handle<Value> insert_sync(const Arguments& args);
    Handle<Value> insert(const Arguments& args, bool sync);

    static Handle<Value> lookup_sync(const Arguments& args);
    Handle<Value> lookup(const Arguments& args, bool sync);

    static Handle<Value> close_sync(const Arguments& args);
    Handle<Value> close(const Arguments& args, bool sync);

    HmSearch* _db;

};

Persistent<Function> HmObject::constructor;
Persistent<Value> HmObject::prototype;


void HmObject::init()
{
    HandleScope scope;

    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("HmObject"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    tpl->PrototypeTemplate()->Set(String::NewSymbol("insertSync"),
                                  FunctionTemplate::New(insert_sync)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("lookupSync"),
                                  FunctionTemplate::New(lookup_sync)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("closeSync"),
                                  FunctionTemplate::New(close_sync)->GetFunction());

    constructor = Persistent<Function>::New(tpl->GetFunction());

    // Get hold of the prototype for our objects so we can do some
    // sanity checking on the objects the caller passes to us later

    Local<Object> obj = constructor->NewInstance();
    prototype = Persistent<Value>::New(obj->GetPrototype());
}


Handle<Value> HmObject::create_with_db(HmSearch *db)
{
    HandleScope scope;

    Local<Value> jsobj = constructor->NewInstance();
    HmObject* obj = ObjectWrap::Unwrap<HmObject>(jsobj->ToObject());
    obj->_db = db;

    return scope.Close(jsobj);
}


HmObject* HmObject::unwrap(Handle<Object> handle)
{
    // Sanity check the object before we accept it as one of our own
    // wrapped objects

    // Basic checks done as asserts by UnWrap()
    if (!handle.IsEmpty() && handle->InternalFieldCount() == 1) {
        // Check the prototype.  This effectively stops inheritance,
        // but since we don't expose a constructor that isn't feasible
        // anyway.
        Handle<Value> objproto = handle->GetPrototype();
        if (objproto == prototype) {
            // OK, this is us
            return ObjectWrap::Unwrap<HmObject>(handle);
        }
    }

    ThrowException(Exception::TypeError(String::New("<this> is not a hmsearch object")));
    return NULL;
}


Handle<Value> HmObject::New(const Arguments& args)
{
    HandleScope scope;

    if (args.IsConstructCall()) {
        // Invoked as constructor: `new HmObject(...)`
        HmObject* obj = new HmObject();
        obj->Wrap(args.This());
        return args.This();
    }
    else {
        // Invoked as plain function `HmObject(...)`, turn into construct call.
        return scope.Close(constructor->NewInstance());
    }
}


Handle<Value> HmObject::insert_sync(const Arguments& args)
{
    HmObject* obj = unwrap(args.This());

    if (obj) {
        return obj->insert(args, true);
    }
    else {
        return Handle<Value>();
    }
}


Handle<Value> HmObject::insert(const Arguments& args, bool sync)
{
    HandleScope scope;

    if (args.Length() != 1) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }

    if (!args[0]->IsString()) {
        ThrowException(Exception::TypeError(String::New("Wrong arguments")));
        return scope.Close(Undefined());
    }

    if (!_db) {
        ThrowException(Exception::Error(String::New("database is closed")));
        return scope.Close(Undefined());
    }

    String::AsciiValue hash(args[0]);

    if (_db->insert(HmSearch::parse_hexhash(*hash))) {
        return scope.Close(Undefined());
    }
    else {
        ThrowException(Exception::Error(String::New(_db->get_last_error())));
        return scope.Close(Undefined());
    }
}


Handle<Value> HmObject::lookup_sync(const Arguments& args)
{
    HmObject* obj = unwrap(args.This());

    if (obj) {
        return obj->lookup(args, true);
    }
    else {
        return Handle<Value>();
    }
}


Handle<Value> HmObject::lookup(const Arguments& args, bool sync)
{
    HandleScope scope;

    if (args.Length() < 1 || args.Length() > 2) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }

    if (!args[0]->IsString() ||
        (args.Length() > 1 && !args[1]->IsNumber())) {
        ThrowException(Exception::TypeError(String::New("Wrong arguments")));
        return scope.Close(Undefined());
    }

    if (!_db) {
        ThrowException(Exception::Error(String::New("database is closed")));
        return scope.Close(Undefined());
    }

    String::AsciiValue hash(args[0]);
    int max_error = args.Length() > 1 ? args[1]->IntegerValue() : -1;

    HmSearch::LookupResultList matches;
    if (_db->lookup(HmSearch::parse_hexhash(*hash), matches, max_error)) {
        size_t elements = matches.size();
        Local<Array> a = Array::New(elements);

        if (a.IsEmpty()) {
            // error creating the array
            return scope.Close(Undefined());
        }

        int i = 0;
        for (HmSearch::LookupResultList::const_iterator res = matches.begin();
             res != matches.end();
             ++res, ++i) {
            Local<Object> obj = Object::New();
            std::string hexhash = HmSearch::format_hexhash(res->hash);
            obj->Set(String::NewSymbol("hash"), String::New(hexhash.c_str()));
            obj->Set(String::NewSymbol("distance"), Integer::New(res->distance));
            a->Set(i, obj);
        }

        return scope.Close(a);
    }
    else {
        ThrowException(Exception::Error(String::New(_db->get_last_error())));
        return scope.Close(Undefined());
    }
}


Handle<Value> HmObject::close_sync(const Arguments& args)
{
    HmObject* obj = unwrap(args.This());

    if (obj) {
        return obj->close(args, true);
    }
    else {
        return Handle<Value>();
    }
}


Handle<Value> HmObject::close(const Arguments& args, bool sync)
{
    HandleScope scope;

    if (args.Length() != 0) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }

    if (_db) {
        if (!_db->close()) {
            ThrowException(Exception::Error(String::New(_db->get_last_error())));
        }

        delete _db;
        _db = NULL;
    }

    return scope.Close(Undefined());
}


/*
 * Module interface
 */

static Handle<Value> init(const Arguments& args, bool sync)
{
    HandleScope scope;

    if (args.Length() != 4) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }

    if (!args[0]->IsString() || !args[1]->IsNumber() || !args[2]->IsNumber() || !args[3]->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("Wrong arguments")));
        return scope.Close(Undefined());
    }

    String::Utf8Value path(args[0]);
    unsigned hash_bits = args[1]->IntegerValue();
    unsigned max_error = args[2]->IntegerValue();
    uint64_t num_hashes = args[3]->NumberValue();
    std::string error_msg;

    if (!HmSearch::init(*path, hash_bits, max_error, num_hashes, &error_msg)) {
        ThrowException(Exception::Error(String::New(error_msg.c_str())));
    }

    return scope.Close(Undefined());
}

static Handle<Value> hmsearch_initSync(const Arguments& args) {
    return init(args, true);
}


static Handle<Value> open(const Arguments& args, bool sync) {
    HandleScope scope;

    if (args.Length() != 2) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }

    if (!args[0]->IsString() || !args[1]->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("Wrong arguments")));
        return scope.Close(Undefined());
    }

    String::Utf8Value path(args[0]);
    HmSearch::OpenMode mode = HmSearch::OpenMode(args[1]->IntegerValue());
    std::string error_msg;

    HmSearch* db = HmSearch::open(*path, mode, &error_msg);

    if (db) {
        return scope.Close(HmObject::create_with_db(db));
    }
    else {
        ThrowException(Exception::Error(String::New(error_msg.c_str())));
        return scope.Close(Undefined());
    }
}


static Handle<Value> hmsearch_openSync(const Arguments& args) {
    return open(args, true);
}

void module_init(Handle<Object> exports) {
    HmObject::init();

    exports->Set(String::NewSymbol("READONLY"),
                 Integer::New(HmSearch::READONLY));

    exports->Set(String::NewSymbol("READWRITE"),
                 Integer::New(HmSearch::READWRITE));

    exports->Set(String::NewSymbol("initSync"),
                 FunctionTemplate::New(hmsearch_initSync)->GetFunction());

    exports->Set(String::NewSymbol("openSync"),
                 FunctionTemplate::New(hmsearch_openSync)->GetFunction());
}

NODE_MODULE(hmsearch, module_init)


/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/
