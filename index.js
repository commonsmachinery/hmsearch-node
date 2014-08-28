'use strict';

var hmsearch;
if (process.platform === "win32" && process.arch === "x64") {
    hmsearch = require('./win32/x64/hmsearch');
}
else if (process.platform === "win32" && process.arch === "ia32") {
    hmsearch = require('./win32/ia32/hmsearch');
}
else {
    hmsearch = require('./build/Release/hmsearch');
}

module.exports = hmsearch;

