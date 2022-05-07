// If you know how to do this properly feel free to open a PR.
// This is a quick and very dirty script to change the theme of the page.

// Icons Downloaded from : https://remixicon.com/
const MOON = `<svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" width="24" height="24"><path fill="none" d="M0 0h24v24H0z"/><path d="M12 22C6.477 22 2 17.523 2 12S6.477 2 12 2s10 4.477 10 10-4.477 10-10 10zm-6.671-5.575A8 8 0 1 0 16.425 5.328a8.997 8.997 0 0 1-2.304 8.793 8.997 8.997 0 0 1-8.792 2.304z"/></svg>`;
const SUN = `<svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" width="24" height="24"><path fill="none" d="M0 0h24v24H0z"/><path d="M12 18a6 6 0 1 1 0-12 6 6 0 0 1 0 12zm0-2a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM11 1h2v3h-2V1zm0 19h2v3h-2v-3zM3.515 4.929l1.414-1.414L7.05 5.636 5.636 7.05 3.515 4.93zM16.95 18.364l1.414-1.414 2.121 2.121-1.414 1.414-2.121-2.121zm2.121-14.85l1.414 1.415-2.121 2.121-1.414-1.414 2.121-2.121zM5.636 16.95l1.414 1.414-2.121 2.121-1.414-1.414 2.121-2.121zM23 11v2h-3v-2h3zM4 11v2H1v-2h3z"/></svg>`;

function createThemeButton() {
  let div = document.createElement('button');
  div.innerHTML  = "";
  div.innerHTML += `<div class="sidebar-toggle-button" id="icon-moon" style="display:none">${MOON.trim()}</div>`;
  div.innerHTML += `<div class="sidebar-toggle-button" id="icon-sun">${SUN.trim()}</div>`;
  div.classList.add('sidebar-toggle');
  div.classList.add('theme');
  return div;
}

function toggleTheme() {
  function _toggleTheme(enable, disable) {
    document.querySelectorAll(`.theme-${enable}`).forEach(function(el) {
      el.removeAttribute("disabled");
    });
    document.querySelectorAll(`.theme-${disable}`).forEach(function(el) {
      el.setAttribute("disabled", '');
    });
  }
  // Yup, I have no idea how JS/CSS works together.
  const isLight = !document.querySelector(".theme-light").hasAttribute("disabled");
  if (isLight) {
    _toggleTheme('dark', 'light');
    document.querySelector("#icon-sun").style.display = 'flex';
    document.querySelector("#icon-moon").style.display = 'none';
  } else {
    _toggleTheme('light', 'dark');
    document.querySelector("#icon-moon").style.display = 'flex';
    document.querySelector("#icon-sun").style.display = 'none';
  }
}

function onDocsifyReady() {
  // TODO: store the theme in the localStorage.

  let theme_toggle = createThemeButton();
  let main = document.querySelector('main');
  let sidebar_toggle = main.querySelector('.sidebar-toggle');
  main.insertBefore(theme_toggle, sidebar_toggle);
  theme_toggle.onclick = function() {
    toggleTheme();
  }
}
