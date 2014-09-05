/* Test suite for hmsearch async API

   Copyright 2014 Commons Machinery http://commonsmachinery.se/
   Distributed under an MIT license, please see LICENSE in the top dir.
*/

/* global describe, it, beforeEach, afterEach */

'use strict';

var BPromise = require('bluebird');
var expect = require('expect.js');
var fs = require('fs');
var temp = require('temp');
var hmsearch = require('../index');

var initAsync = BPromise.promisify(hmsearch.init);
var openAsync = BPromise.promisify(hmsearch.open);

var hashes, lookup_distance_4;

describe('async', function() {
    describe('init', function() {
        describe('bad calls', function() {
            it('should throw exception on bad arguments', function() {
                expect( hmsearch.init ).withArgs( )
                    .to.throwException( TypeError );
                expect( hmsearch.init ).withArgs( 1, 2, 3, 4, {} )
                    .to.throwException( TypeError );
            });

            it('should pass error to callback on invalid path', function(done) {
                hmsearch.init('/I/hope/this/wont/exist.kch', 64, 5, 100, function(err) {
                    expect( err ).to.be.an( Error );
                    done();
                });
            });
        });

        describe('successful calls', function() {
            var path1, path2;

            beforeEach(function() {
                path1 = temp.path({ suffix: '.kch' });
                path2 = temp.path({ suffix: '.kch' });
            });

            afterEach(function() {
                if (fs.existsSync(path1)) { fs.unlinkSync(path1); }
                if (fs.existsSync(path2)) { fs.unlinkSync(path2); }
            });

            it('should create multiple new files in parallel', function() {
                return BPromise.all([
                    initAsync(path1, 64, 5, 100).then(function() {
                        expect( fs.existsSync(path1) ).to.be.ok();
                    }),
                    initAsync(path2, 64, 5, 100).then(function() {
                        expect( fs.existsSync(path2) ).to.be.ok();
                    })
                ]);
            });
        });
    });

    describe('open', function() {
        describe('bad calls', function() {
            it('should throw exception on bad arguments', function() {
                expect( hmsearch.open ).withArgs( 'foo.kch' )
                    .to.throwException( TypeError );
                expect( hmsearch.open ).withArgs( 0, 'foo.kch', {} )
                    .to.throwException( TypeError );
            });

            it('should pass error to callback on invalid path', function(done) {
                hmsearch.open('/I/hope/this/wont/exist.kch', hmsearch.READONLY, function(err, db) {
                    expect( err ).to.be.an( Error );
                    expect( db ).to.be( undefined );
                    done();
                });
            });
        });

        describe('successful calls', function() {
            var path1, path2;

            beforeEach(function() {
                path1 = temp.path({ suffix: '.kch' });
                hmsearch.initSync(path1, 64, 5, 100);
                expect( fs.existsSync(path1) ).to.be.ok();

                path2 = temp.path({ suffix: '.kch' });
                hmsearch.initSync(path2, 64, 5, 100);
                expect( fs.existsSync(path2) ).to.be.ok();
            });

            afterEach(function() {
                if (fs.existsSync(path1)) { fs.unlinkSync(path1); }
                if (fs.existsSync(path2)) { fs.unlinkSync(path2); }
            });

            it('should open databases', function() {

                return BPromise.all([
                    openAsync(path1, hmsearch.READONLY).then(function(db) {
                        expect( db ).to.be.ok();
                        expect( db.open ).to.be( true );

                        // Sync close is good enough here
                        db.closeSync();
                    }),

                    openAsync(path2, hmsearch.READONLY).then(function(db) {
                        expect( db ).to.be.ok();
                        expect( db.open ).to.be( true );

                        db.closeSync();
                    }),
                ]);
            });
        });
    });

    describe('close', function() {
        var path, db;

        beforeEach(function() {
            path = temp.path({ suffix: '.kch' });
            hmsearch.initSync(path, 64, 5, 100);

            db = hmsearch.openSync(path, hmsearch.READONLY);
            expect( db ).to.be.ok();
            expect( db.open ).to.be( true );
            BPromise.promisifyAll(db);
        });

        afterEach(function() {
            if (db.open) {
                db.closeSync();
            }
            if (fs.existsSync(path)) {
                fs.unlinkSync(path);
            }
        });

        it('should throw exception on bad arguments', function() {
            expect( db.close ).withArgs( )
                .to.throwException( TypeError );
            expect( db.close ).withArgs( {} )
                .to.throwException( TypeError );
        });

        it('should handle multiple parallel closes', function() {
            var promises = [];
            var check = function() {
                expect( db.open ).to.be( false );
            };
            for (var i = 0; i < 50; i++) {
                promises.push(db.closeAsync().then(check));
            }

            return BPromise.all(promises);
        });
    });


    describe('insert', function() {
        var path, db;

        beforeEach(function() {
            path = temp.path({ suffix: '.kch' });
            hmsearch.initSync(path, 64, 5, 100);

            db = hmsearch.openSync(path, hmsearch.READWRITE);
            expect( db ).to.be.ok();
            expect( db.open ).to.be( true );
            BPromise.promisifyAll(db);
        });

        afterEach(function() {
            if (db.open) {
                db.closeSync();
            }
            if (fs.existsSync(path)) {
                fs.unlinkSync(path);
            }
        });

        it('should throw exception on bad arguments', function() {
            expect( db.insert ).withArgs( )
                .to.throwException( TypeError );
            expect( db.insert ).withArgs( '414F2C9F12625841', {} )
                .to.throwException( TypeError );
        });

        it('should callback with error on closed database', function(done) {
            db.closeSync();
            expect( db.open ).to.be( false );

            db.insert('414F2C9F12625841', function(err) {
                expect( err ).to.be.an( Error );
                expect( err.message ).to.be( "database is closed" );
                done();
            });
        });

        it('should add hashes in parallel', function() {
            return BPromise.map(
                hashes,
                function(hash) {
                    return db.insertAsync(hash);
                },
                { concurrency: hashes.length }
            );
        });

        it('should not crash when database is closed while inserting', function() {
            return BPromise.map(
                hashes,
                function(hash, i) {
                    if (i === 10) {
                        return db.closeAsync();
                    }
                    else
                    {
                        return db.insertAsync(hash)
                            .error(function(err) {
                                expect( err.message ).to.be( "database is closed" );
                            });
                    }
                },
                { concurrency: hashes.length }
            );
        });
    });

    describe('lookup', function() {
        var path, db;

        beforeEach(function() {
            path = temp.path({ suffix: '.kch' });
            hmsearch.initSync(path, 64, 5, 100);

            db = hmsearch.openSync(path, hmsearch.READWRITE);
            expect( db ).to.be.ok();
            expect( db.open ).to.be( true );
            BPromise.promisifyAll(db);

            for (var i = 0; i < hashes.length; i++) {
                db.insertSync(hashes[i]);
            }
        });

        afterEach(function() {
            if (db.open) {
                db.closeSync();
            }
            if (fs.existsSync(path)) {
                fs.unlinkSync(path);
            }
        });

        it('should throw exception on bad arguments', function() {
            expect( db.lookup ).withArgs( )
                .to.throwException( TypeError );
            expect( db.lookup ).withArgs( '414F2C9F12625841', {} )
                .to.throwException( TypeError );
            expect( db.lookup ).withArgs( '414F2C9F12625841', -1, {} )
                .to.throwException( TypeError );
            expect( db.lookup ).withArgs( '414F2C9F12625841', 'foo', function(){} )
                .to.throwException( TypeError );
        });

        it('should callback with error on closed database', function(done) {
            db.closeSync();
            expect( db.open ).to.be( false );

            db.lookup('414F2C9F12625841', function(err, res) {
                expect( err ).to.be.an( Error );
                expect( err.message ).to.be( "database is closed" );
                done();
            });
        });

        it('should find match with distance', function() {
            return db.lookupAsync(lookup_distance_4)
                .then(function(res) {
                    expect( res ).to.have.length( 1 );
                    expect( res[0].hash.toUpperCase() ).to.be( hashes[0] );
                    expect( res[0].distance ).to.be( 4 );
                });
        });

        it('should lookup with reduced max_error', function() {
            return db.lookupAsync(lookup_distance_4, 3)
                .then(function(res) {
                    expect( res ).to.have.length( 0 );
                });
        });

        it('should lookup in parallel', function() {
            return BPromise.map(
                hashes,
                function(hash) {
                    return db.lookupAsync(hash)
                        .then(function(res) {
                            expect( res ).to.have.length( 1 );
                            expect( res[0].hash.toUpperCase() ).to.be( hash );
                            expect( res[0].distance ).to.be( 0 );

                            return 1;
                        });
                },
                { concurrency: hashes.length })
                .reduce(function(total, current) {
                    return total + current;
                }, 0)
                .then(function(matches) {
                    expect( matches ).to.be( hashes.length );
                });
        });

        it('should not crash when database is closed while looking up', function() {
            return BPromise.map(
                hashes,
                function(hash, i) {
                    if (i === 10) {
                        return db.closeAsync();
                    }
                    else
                    {
                        return db.lookupAsync(hash)
                            .then(function(res) {
                                expect( res ).to.have.length( 1 );
                                expect( res[0].hash.toUpperCase() ).to.be( hash );
                                expect( res[0].distance ).to.be( 0 );
                            })
                            .error(function(err) {
                                expect( err.message ).to.be( "database is closed" );
                            });
                    }
                },
                { concurrency: hashes.length }
            );
        });
    });
});


hashes = [
    '0593BA4CA56D2979',
    '575E2695E44A138F',
    'A7C2F44E052B3768',
    'BAA1DE1B5FB02F79',
    '39E6E7C831AC8A0F',
    '50D55F807DBA6925',
    'F21FC6DACF697C5F',
    '6F56EFF83C424D2F',
    'C84297CC5558C1B9',
    'F29F018F687DCEF1',
    'FADDD01FE7493281',
    'B50A0C5381653D68',
    '1802032E08F2EDBA',
    'E726243D38501FE7',
    'E702DFAE03BED5D7',
    '4AFEF76C6C1CF63C',
    'EA1A02BF04F3E764',
    '8E3F894AA49FF85C',
    '0A962E8A9E3C0360',
    'A2DAA2F78EFD2BF0',
    '66A0F1F3460EBC3F',
    '224283B8B61A1BDA',
    'F7C02245A3EEC1BD',
    '949AF95E539AF8AA',
    '59952F4D10EB9909',
    '072E3437E3A08EB5',
    'E77B10847F24F982',
    'FDD82EA6B4C85572',
    '66B0F4C7433CFA90',
    'CEB56AAC58A97681',
    'BBA6C3280BD572EF',
    '9D3A2220B4664572',
    'FFEA2D63735093DC',
    'FA2E13B609CD5B4F',
    '9E3E5992AC26D4D7',
    'FDF234FE8C05DAC9',
    '5E090BE594FE73BA',
    'E7F29B7DFBD54147',
    'EDCB458512A0BFA3',
    '663140A903F97AA1',
    '3A694697DCC780AC',
    '03E1357B8D033911',
    '437213956F125335',
    '2DBB89D425AF1D30',
    '0E96D71BBFE53A91',
    'D97469D0A892A551',
    'AF2EF2E7A970B237',
    '28D3153C5EF836F4',
    '0476663BB52CA704',
    'D125B89AA24BEA28',
];

lookup_distance_4 = '0590BA0CA56D0979';
