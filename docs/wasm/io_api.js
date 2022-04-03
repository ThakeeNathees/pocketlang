/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

// This will link the javascript C api function.
// Add the option '--js-library pocketlang.js' to link at compiling.
// Reference: https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#implement-a-c-api-in-javascript

mergeInto(LibraryManager.library, {
  /** js_func_name : function() {...} */

  js_errorPrint : function(type, line, message) {
    var err_text = ''
    const msg = AsciiToString(message);
    if (type == 0 /*PK_ERROR_COMPILE*/) {
      err_text = `[Error at:${line}] ${msg}`;
    } else if (type == 1 /*PK_ERROR_RUNTIME*/) {
      err_text = `Error: ${msg}`;
    } else if (type == 2 /*PK_ERROR_STACKTRACE*/) {
      err_text = `  [at:${line}] ${msg}`;
    }

    var out = document.getElementById("code-output");
    // To Indicate error (should be removed before each run request).
    out.classList.add("has-error");
    out.innerText += err_text + '\n';
  },

  js_writeFunction : function(message) {
    var out = document.getElementById("code-output");
    out.innerText += AsciiToString(message)
  },
});
