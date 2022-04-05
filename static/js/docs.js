

$("#toggle-menu").onclick = function() {
  $("#navigation").classList.toggle("collapsed");
}

$$(".nav-section").forEach(function(el) {
  el.onclick = function() {
    el.classList.toggle("collapsed");
    el.nextElementSibling.classList.toggle("collapsed");
  }
});
