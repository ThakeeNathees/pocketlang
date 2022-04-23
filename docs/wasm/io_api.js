/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

// This will link the javascript C api function.
// Add the option '--js-library pocketlang.js' to link at compiling.
// Reference: https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#implement-a-c-api-in-javascript

mergeInto(LibraryManager.library, {
  /** js_func_name : function() {...} */

  js_errorPrint : function(message) {
    var out = document.getElementById("code-output");
    // To Indicate error (should be removed before each run request).
    out.classList.add("has-error");

    let msg = AsciiToString(message);
    out.innerHTML += `<span class="error-line">${msg}</span>`;
  },

  js_writeFunction : function(message) {
    var out = document.getElementById("code-output");
    out.innerText += AsciiToString(message);
  },
});
