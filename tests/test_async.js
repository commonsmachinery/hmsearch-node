/* Test suite for hmsearch async API

   Copyright 2014 Commons Machinery http://commonsmachinery.se/
   Distributed under an MIT license, please see LICENSE in the top dir.
*/

/* global describe, it, before, after */

'use strict';

var expect = require('expect.js');
var fs = require('fs');
var temp = require('temp');
var hmsearch = require('../index');


describe('async', function() {
    describe('init', function() {
        describe('bad calls', function() {
            it('should throw exception on bad arguments', function() {
                expect( hmsearch.init ).withArgs( )
                    .to.throwException( Error );
                expect( hmsearch.init ).withArgs( 1, 2, 3, 4, {} )
                    .to.throwException( Error );
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

            before(function() {
                path1 = temp.path({ suffix: '.kch' });
                path2 = temp.path({ suffix: '.kch' });
            });

            after(function() {
                if (fs.existsSync(path1)) { fs.unlinkSync(path1); }
                if (fs.existsSync(path2)) { fs.unlinkSync(path2); }
            });

            it('should create multiple new files in parallel', function(done) {
                var calls = 2;

                hmsearch.init(path1, 64, 5, 100, function(err) {
                    expect( err ).to.be( undefined );
                    expect( fs.existsSync(path1) ).to.be.ok();

                    if (--calls <= 0) { done(); }
                });

                hmsearch.init(path2, 64, 5, 100, function(err) {
                    expect( err ).to.be( undefined );
                    expect( fs.existsSync(path2) ).to.be.ok();

                    if (--calls <= 0) { done(); }
                });
            });
        });
    });

    describe('open', function() {
        describe('bad calls', function() {
            it('should throw exception on bad arguments', function() {
                expect( hmsearch.open ).withArgs( 'foo.kch' )
                    .to.throwException( Error );
                expect( hmsearch.open ).withArgs( 0, 'foo.kch', {} )
                    .to.throwException( Error );
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

            before(function() {
                path1 = temp.path({ suffix: '.kch' });
                hmsearch.initSync(path1, 64, 5, 100);
                expect( fs.existsSync(path1) ).to.be.ok();

                path2 = temp.path({ suffix: '.kch' });
                hmsearch.initSync(path2, 64, 5, 100);
                expect( fs.existsSync(path2) ).to.be.ok();
            });

            after(function() {
                if (fs.existsSync(path1)) { fs.unlinkSync(path1); }
                if (fs.existsSync(path2)) { fs.unlinkSync(path2); }
            });

            it('should open and close databases', function(done) {
                var calls = 2;

                hmsearch.open(path1, hmsearch.READONLY, function(err, db) {
                    expect( err ).to.be( null );
                    expect( db ).to.be.ok();

                    // TODO: async close
                    expect( db ).to.have.property( 'closeSync' );
                    db.closeSync();

                    if (--calls <= 0) { done(); }
                });

                hmsearch.open(path2, hmsearch.READONLY, function(err, db) {
                    expect( err ).to.be( null );
                    expect( db ).to.be.ok();

                    // TODO: async close
                    expect( db ).to.have.property( 'closeSync' );
                    db.closeSync();

                    if (--calls <= 0) { done(); }
                });
            });
        });
    });
});
