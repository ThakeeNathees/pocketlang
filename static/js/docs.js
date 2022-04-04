
let $ = document.querySelector.bind(document);
let $$ = document.querySelectorAll.bind(document);

// Syntax highlighting.
document.addEventListener('DOMContentLoaded', (event) => {
  $$('.highlight').forEach(function(el) {
    hljs.highlightElement(el);
  });
});

$("#toggle-menu").onclick = function() {
  $("#navigation").classList.toggle("collapsed");
}

$$(".nav-section").forEach(function(el) {
  el.onclick = function() {
    el.classList.toggle("collapsed");
    el.nextElementSibling.classList.toggle("collapsed");
  }
});
