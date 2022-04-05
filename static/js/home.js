
window.addEventListener('scroll', function() {
  if (window.scrollY > 30) {
    $("#navbar").classList.add("stick");
  } else {
    $("#navbar").classList.remove("stick");
  }
});
