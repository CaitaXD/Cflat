(function(){
  const root = document.documentElement;
  const stored = localStorage.getItem('cflatdoc-theme');
  if (stored) root.setAttribute('data-theme', stored);
  const btn = document.getElementById('theme-toggle');
  if (btn) btn.addEventListener('click', () => {
    const cur = root.getAttribute('data-theme') === 'light' ? 'dark' : 'light';
    root.setAttribute('data-theme', cur);
    localStorage.setItem('cflatdoc-theme', cur);
  });

  const search = document.getElementById('search');
  if (search) {
    const links = Array.from(document.querySelectorAll('.nav-link'));
    search.addEventListener('input', () => {
      const q = search.value.trim().toLowerCase();
      links.forEach(a => {
        const m = !q || a.textContent.toLowerCase().includes(q);
        a.classList.toggle('hidden', !m);
      });
    });
    document.addEventListener('keydown', e => {
      if ((e.metaKey || e.ctrlKey) && e.key === 'k') { e.preventDefault(); search.focus(); }
    });
  }

  document.querySelectorAll('.copy').forEach(b => {
    b.addEventListener('click', () => {
      const code = b.parentElement.querySelector('pre').innerText;
      navigator.clipboard.writeText(code).then(() => {
        b.classList.add('ok'); b.textContent = 'copied';
        setTimeout(() => { b.classList.remove('ok'); b.textContent = 'copy'; }, 1200);
      });
    });
  });
})();
