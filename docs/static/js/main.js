
let $ = document.querySelector.bind(document);
let $$ = document.querySelectorAll.bind(document);

// Syntax highlighting.
document.addEventListener('DOMContentLoaded', (event) => {
  $$('.highlight').forEach(function(el) {
    hljs.highlightElement(el);
  });
});
