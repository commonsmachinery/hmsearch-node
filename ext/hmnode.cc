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

    static Handle<Value> create_with_db(HmSearch *db);

private:
    HmObject() : _db(NULL) { }
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

    HmSearch* _db;
};

Persistent<Function> HmObject::constructor;
Persistent<FunctionTemplate> HmObject::prototype;


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


Handle<Value> HmObject::create_with_db(HmSearch *db)
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


void module_init(Handle<Object> exports) {
    HmObject::init();

    exports->Set(NanNew<String>("READONLY"),
                 NanNew<Integer>(HmSearch::READONLY));

    exports->Set(NanNew<String>("READWRITE"),
                 NanNew<Integer>(HmSearch::READWRITE));

    exports->Set(NanNew<String>("initSync"),
                 NanNew<FunctionTemplate>(init_sync)->GetFunction());

    exports->Set(NanNew<String>("openSync"),
                 NanNew<FunctionTemplate>(open_sync)->GetFunction());
}

NODE_MODULE(hmsearch, module_init)


/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/
