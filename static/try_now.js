/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

const _initial_snippet = `\
# A recursive fibonacci function.
def fib(n)
  if n < 2 then return n end
  return fib(n-1) + fib(n-2)
end

# Print all fibonacci from 0 to 10 exclusive.
for i in 0..10
  print(fib(i))
end
`

var highlight_fn = function(editor) {
  // highlight.js does not trim old tags,
  // let's do it by this hack.
  editor.textContent = editor.textContent;
  editor.innerHTML = Prism.highlight(editor.textContent,
                                     Prism.languages.ruby, 'ruby');
}

var runSource;

// called after index.html is loaded -> Module is defined.
window.onload = function() {
  runSource = Module.cwrap('runSource', 'number', ['string']);
  document.getElementById("run-button").onclick = function() {
    document.getElementById('output').innerText = '';
    const source = document.querySelector('.editor').textContent;
    runSource(source);
  }

  let editor = document.querySelector('.editor')
  editor.textContent = _initial_snippet;
  highlight_fn(editor);
}

