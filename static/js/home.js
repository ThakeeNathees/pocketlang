
let $ = document.querySelector.bind(document);
let $$ = document.querySelectorAll.bind(document);

const initial_code = `print('Hello World!')`

// Syntax highlighting.
document.addEventListener('DOMContentLoaded', (event) => {
  $$('.highlight').forEach(function(el) {
    hljs.highlightElement(el);
  });
});

let code_highlight_fn = withLineNumbers(function(editor) {
  editor.textContent = editor.textContent;
  html = hljs.highlight(editor.textContent, {language : 'ruby'}).value;
  editor.innerHTML = html;
});

let code_editor = $("#code-editor");
CodeJar(code_editor, code_highlight_fn, { indentOn: /[(\[{]$/});

var runSource; // Native function.

window.onload = function() {
  code_editor.textContent = initial_code;
  code_highlight_fn(code_editor);

  // Module will be defined by pocketlang.js when the page is loaded.
  runSource = Module.cwrap('runSource', 'number', ['string']);
  $("#run-button").onclick = function() {
    $("#code-output").classList.remove("has-error");
    $('#code-output').innerText = '';
    const source = code_editor.textContent;
    runSource(source);
  }
}
