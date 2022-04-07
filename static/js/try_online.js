

let examples = {
  "Simple" :`\
# A recursive fibonacci function.
def fib(n)
  if n < 2 then return n end
  return fib(n-1) + fib(n-2)
end

# Prints all fibonacci from 0 to 10 exclusive.
for i in 0..10
  print(\"fib($i) = \${fib(i)}\")
end`,

  "Concurrent" :   `\
import Fiber

def fn()
  print('running')
  yield(42) # Return 42.
  print('resumed')
end

fb = Fiber.new(fn)
val = Fiber.run(fb) # Prints 'running'.
print(val)          # Prints 42.
Fiber.resume(fb)    # Prints 'resumed'.`,

  "Object-Oriented" :   `\
## NOTE THAT CLASSES WIP AND WILL BE CHANGED.

class _Vector
  x = 0; y = 0
end

def Vector(x, y)
  v = _Vector()
  v.x = x; v.y = y
  return v
end

def add(v1, v2)
  return Vector(v1.x + v2.x,
                v1.y + v2.y)
end

v1 = Vector(1, 2)
v2 = Vector(3, 4)
print(add(v1, v2))

`,
}

let code_editor = $("#code-editor");
let code_example_titles = $$(".code-example-title");

let code_highlight_fn = withLineNumbers(function(editor) {
  editor.textContent = editor.textContent;
  html = hljs.highlight(editor.textContent, {language : 'ruby'}).value;
  
  // https://github.com/antonmedv/codejar/issues/22#issuecomment-773894139
  if (html.length > 2 && html.substring(html.length - 2, html.length) != '\n\n') {
    html += '\n'
  }

  editor.innerHTML = html;
});

function setCode(el) {
  code_example_titles.forEach(function(el) {
    el.classList.remove("active");
  });
  el.classList.add("active");
  code_editor.textContent = examples[el.getAttribute("name")];
  code_highlight_fn(code_editor);
}

setCode(code_example_titles[0]);

code_example_titles.forEach(function(el) {
  el.onclick = function() {
    setCode(el);
  }
});

CodeJar(code_editor, code_highlight_fn, { indentOn: /[(\[{]$/});

var runSource; // Native function.
window.onload = function() {
  // Module will be defined by pocketlang.js when the page is loaded.
  runSource = Module.cwrap('runSource', 'number', ['string']);
  $("#run-button").onclick = function() {
    $("#code-output").classList.remove("has-error");

    $('#code-output').innerText = '';
    const source = code_editor.textContent;
    runSource(source);
  }
}
