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

The API only supports synchronous methods now, but callback methods
will be added soon.

The synchronous methods all throw an `Error` if the operation fails,
with the message providing further details.

### Initialise a database

    hmsearch.initdbSync(path, hash_bits, max_error, num_hashes)


### Open a database

    db = hmsearch.openSync(path, mode)

Mode is either `hmsearch.READONLY` or `hmsearch.READWRITE`.

Returns a new database object.


### Close a database

    db.closeSync()

Sync any changes to the database and close it.  This must be done to
release any locks on the database so other processes can access it.

It is safe to close the database multiple times.

The database is also closed when the database object is garbage
collected.


### Insert a hash

    db.insertSync(hash)

`hash` must be a hexadecimal string of the correct length.


### Lookup a hash

    matches = db.lookupSync(query, [max_error])

`query` must be a hexadecimal string of the correct length.  If
`max_error` is provided and non-negative, it can further restrict the
accepted hamming distance than the database default.

`matches` lists each matching hash as a hexadecimal string and the
hamming distance to the `query` hash:

    [ { hash: '0123456789abcdef', distance: 3 }, ... ]

An empty list is returned if no matches are found.


License
-------

Copyright 2014 Commons Machinery http://commonsmachinery.se/

Distributed under an MIT license, please see LICENSE in the top dir.

Contact: dev@commonsmachinery.se
