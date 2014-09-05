hmsearch-node
=============

Node API to HmSearch library.  For more information on the library,
see https://github.com/commonsmachinery/hmsearch

Installing
----------

Currently the build only supports Linux.  Pull requests for Mac and
Windows are welcome!

Ensure that Kyoto Cabinet is installed. On Ubuntu:

    apt-get install libkyotocabinet-dev kyotocabinet-util

When building from the git repository the `hmsearch` submodule must be
initialised on first checkout:

    git submodule init
    git submodule update

API
---

For details on these methods, see the documentation for the C++
library:
https://github.com/commonsmachinery/hmsearch/blob/master/hmsearch.h

The synchronous methods all throw an `Error` if the operation fails,
with the message providing further details.  The asynchronous methods
instead invoke the callback with the first argument set to an `Error`
instance.


### Initialise a database

    hmsearch.initdb(path, hash_bits, max_error, num_hashes, function(err) {...})

    hmsearch.initdbSync(path, hash_bits, max_error, num_hashes)


### Open a database

    hmsearch.open(path, mode, function(err, db) {...})

    db = hmsearch.openSync(path, mode)

Mode is either `hmsearch.READONLY` or `hmsearch.READWRITE`.

`db` is a newly opened database object.


### Close a database

    db.close(function(err) {...})

    db.closeSync()

Sync any changes to the database and close it.  This must be done to
release any locks on the database so other processes can access it.

It is safe to close the database multiple times.

The database is also closed when the database object is garbage
collected.


### Insert a hash

    db.insert(hash, function(err) {...})

    db.insertSync(hash)

`hash` must be a hexadecimal string of the correct length.


### Lookup a hash

    db.lookupSync(query, [max_error], function(err, matches) {...})

    matches = db.lookupSync(query, [max_error])

`query` must be a hexadecimal string of the correct length.  If
`max_error` is provided and non-negative, it can further restrict the
accepted hamming distance than the database default.

`matches` lists each matching hash as a hexadecimal string and the
hamming distance to the `query` hash:

    [ { hash: '0123456789abcdef', distance: 3 }, ... ]

`matches` is an empty list if no hashes are found.


License
-------

Copyright 2014 Commons Machinery http://commonsmachinery.se/

Distributed under an MIT license, please see LICENSE in the top dir.

Contact: dev@commonsmachinery.se
