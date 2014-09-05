/* Test suite for hmsearch sync API

   Copyright 2014 Commons Machinery http://commonsmachinery.se/
   Distributed under an MIT license, please see LICENSE in the top dir.
*/

/* global describe, it, before, after, beforeEach, afterEach */

'use strict';

var expect = require('expect.js');
var fs = require('fs');
var temp = require('temp');
var hmsearch = require('../index');

var inserted_hashes = [
    '414F2C9F12625841',
    'DD29B9354FE2BB80',
    'D68F2C1AF77285D9',
    'CBCD1C9D73BA4B18',
    '824EB2B44456DBCD',
    'CD9F03E038366F16',
    '6E32A6B749EDCEC5',
    'D81FBE9ADBF9DCAD',
    '78364579589F20D3',
    '3CA1063EC8C29C2C',
    '3CA1063EC8C29C20', // near duplicates
    '3CA1063EC8C29C21', // near duplicates
];

// We accept up to 5 errors
var lookup_distances = [
    '414F2C9F12625841', // exact
    'DD29B9354FE0BB80', // 1
    'D68F0C1AF77085D9', // 2
    'CBCD0C9D73B04B18', // 3
    '422EB2B44456DBCD', // 4
    'CD9F030038066F16', // 5
    '6E32A60049EDCEC5', // 6
];

var non_existing = '92EFC9B23AA80608';

var similar = [
    { hash: '3CA1063EC8C29C20', distance: 0 },
    { hash: '3CA1063EC8C29C21', distance: 1 },
    { hash: '3CA1063EC8C29C2C', distance: 2 },
];

describe('sync', function() {
    describe('bad calls to init', function() {
        it('should throw exception on bad arguments', function() {
            expect( hmsearch.initSync ).withArgs( )
                .to.throwException( TypeError );
            expect( hmsearch.initSync ).withArgs( 1, 2, 3, 4 )
                .to.throwException( TypeError );
        });

        it('should throw exception on invalid path', function() {
            expect( hmsearch.initSync ).withArgs( '/I/hope/this/wont/exist.kch', 64, 5, 100)
                .to.throwException( Error );
        });
    });

    describe('init', function() {
        var path;

        before(function() {
            path = temp.path({ suffix: '.kch' });
        });

        after(function() {
            if (fs.existsSync(path)) {
                fs.unlinkSync(path);
            }
        });

        it('should create new file', function() {
            hmsearch.initSync(path, 64, 5, 100);
            expect( fs.existsSync(path) ).to.be.ok();
        });
    });

    describe('bad calls to open', function() {
        it('should throw exception on bad arguments', function() {
            expect( hmsearch.openSync ).withArgs( 'foo.kch' )
                .to.throwException( TypeError );
            expect( hmsearch.openSync ).withArgs( 0, 'foo.kch' )
                .to.throwException( TypeError );
        });

        it('should throw exception on non-existing DB', function() {
            expect( hmsearch.openSync ).withArgs( temp.path({ suffix: '.kch' }), hmsearch.READONLY )
                .to.throwException( Error );
        });
    });


    describe('open', function() {
        var path;

        beforeEach(function() {
            path = temp.path({ suffix: '.kch' });
            hmsearch.initSync(path, 64, 5, 100);
            expect( fs.existsSync(path) ).to.be.ok();
        });

        afterEach(function() {
            if (fs.existsSync(path)) {
                fs.unlinkSync(path);
            }
        });

        it('should open and close database', function() {
            var db = hmsearch.openSync(path, hmsearch.READONLY);
            expect( db ).to.be.ok();
            expect( db.open ).to.be( true );

            db.closeSync();
        });
    });

    describe('insert', function() {
        var path, db;

        before(function() {
            path = temp.path({ suffix: '.kch' });
            hmsearch.initSync(path, 64, 5, 100);
            expect( fs.existsSync(path) ).to.be.ok();
            db = hmsearch.openSync(path, hmsearch.READWRITE);
            expect( db.open ).to.be( true );
        });

        after(function() {
            if (fs.existsSync(path)) {
                fs.unlinkSync(path);
            }
        });

        it('should throw exception on bad arguments', function() {
            var insert = function() { db.insertSync.apply(db, arguments); };
            expect( insert ).withArgs( )
                .to.throwException( TypeError );
            expect( insert ).withArgs( 0 )
                .to.throwException( TypeError );
        });

        it('should throw exception on bad object', function() {
            expect( function() { db.insertSync.apply({}, arguments); } ).withArgs( )
                .to.throwException( TypeError );
        });

        it('should add hashes to the database', function() {
            var i;
            for (i = 0; i < inserted_hashes.length; i++) {
                db.insertSync(inserted_hashes[i]);
            }
        });

        describe('and lookup', function() {
            // Reopen database to ensure it was all persisted

            before(function() {
                db.closeSync();
                expect( db.open ).to.be( false );
                db = hmsearch.openSync(path, hmsearch.READONLY);
                expect( db.open ).to.be( true );
            });

            it('should throw exception on bad arguments', function() {
                var lookup = function() { db.lookupSync.apply(db, arguments); };
                expect( lookup ).withArgs( )
                    .to.throwException( TypeError );
                expect( lookup ).withArgs( 0 )
                    .to.throwException( TypeError );
            });

            it('should throw exception on bad object', function() {
                expect( function() { db.lookupSync.apply({}, arguments); } ).withArgs( )
                    .to.throwException( TypeError );
            });

            it('should not find missing hash', function() {
                var res = db.lookupSync(non_existing);
                expect( res ).to.have.length( 0 );
            });

            it('should find hashes with max error 5', function() {
                var i;
                for (i = 0; i <= 5; i++) {
                    var res = db.lookupSync(lookup_distances[i]);

                    expect( res ).to.have.length( 1 );
                    expect( res[0].hash.toUpperCase() ).to.be( inserted_hashes[i] );
                    expect( res[0].distance ).to.be( i );
                }
            });

            it('should allow smaller max error', function() {
                var i;
                for (i = 0; i <= 5; i++) {
                    var res = db.lookupSync(lookup_distances[i], 3);

                    if (i <= 3) {
                        expect( res ).to.have.length( 1 );
                        expect( res[0].hash.toUpperCase() ).to.be( inserted_hashes[i] );
                        expect( res[0].distance ).to.be( i );
                    }
                    else {
                        expect( res ).to.have.length( 0 );
                    }
                }
            });


            it('should return all matches', function() {
                var res = db.lookupSync(similar[0].hash);
                expect( res ).to.have.length( similar.length );

                var i, j;
                for (i = 0; i < similar.length; i++) {
                    for (j = 0; j < res.length; j++) {
                        if (similar[i].hash === res[i].hash.toUpperCase()) {
                            expect( res[i].distance ).to.be( similar[i].distance );
                            break;
                        }
                    }

                    if (j === res.length) {
                        expect.fail('expected hash not found: ' + similar[i].hash);
                    }
                }
            });


            describe('and close', function() {
                it('should throw exception on bad arguments', function() {
                    var lookup = function() { db.closeSync.apply(db, arguments); };
                    expect( lookup ).withArgs( 0 )
                        .to.throwException( TypeError );
                });

                it('should throw exception on bad object', function() {
                    expect( function() { db.closeSync.apply({}, arguments); } ).withArgs( )
                        .to.throwException( TypeError );
                });

                it('should close the database', function() {
                    db.closeSync();
                    expect( db.open ).to.be( false );

                    expect( function() { db.lookupSync(non_existing); }).withArgs( )
                        .to.throwException( Error );
                });
            });
        });
    });
});
