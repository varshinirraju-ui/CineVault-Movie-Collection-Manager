/*
 * CineVault – browser side.
 * Talks to the C server's JSON API, renders the collection and the linked-list
 * strip, and looks up posters on Wikipedia when a poster link is left blank.
 */
'use strict';

const MIN_YEAR = 1888;
const MAX_YEAR = new Date().getFullYear() + 5;
const DEFAULT_GENRES = [
  'Action', 'Adventure', 'Animation', 'Comedy', 'Crime', 'Documentary', 'Drama',
  'Fantasy', 'Horror', 'Musical', 'Mystery', 'Romance', 'Sci-Fi', 'Thriller',
];

const $ = (id) => document.getElementById(id);
const ui = {
  offline: $('offline'), summary: $('summary'), nodeSize: $('node-size'),
  strip: $('strip'), track: $('track'),
  editor: $('editor'), formTitle: $('form-title'), form: $('movie-form'),
  title: $('title'), genre: $('genre'), year: $('year'), rating: $('rating'), image: $('image'),
  genreOptions: $('genre-options'), autoPoster: $('auto-poster'), formMessage: $('form-message'),
  submit: $('submit-btn'), cancel: $('cancel-btn'),
  count: $('count'), postersBtn: $('posters-btn'), search: $('search'),
  genreFilter: $('genre-filter'), sort: $('sort'), grid: $('grid'), empty: $('empty'),
  confirm: $('confirm'), confirmTitle: $('confirm-title'), toast: $('toast'),
};

const state = {
  q: '', genre: '', sort: 'added',
  editingId: null,
  nodes: [],            // every movie, in linked-list order
  busy: false,
};

const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

/** Create an element: el('p', { class: 'x', onclick: fn }, 'text', child) */
function el(tag, attrs = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(attrs)) {
    if (value === null || value === undefined || value === false) continue;
    if (key === 'class') node.className = value;
    else if (key.startsWith('on')) node.addEventListener(key.slice(2), value);
    else node.setAttribute(key, value === true ? '' : value);
  }
  for (const child of children.flat()) {
    if (child === null || child === undefined || child === false) continue;
    node.append(child instanceof Node ? child : String(child));
  }
  return node;
}

const plural = (n, word) => `${n} ${word}${n === 1 ? '' : 's'}`;

/** Stable hue for a title, so a movie keeps the same stand-in poster colour. */
function hueFor(text) {
  let h = 0;
  for (const ch of text) h = (h * 31 + ch.codePointAt(0)) % 360;
  return h;
}

let toastTimer;
function toast(message, isError = false) {
  ui.toast.textContent = message;
  ui.toast.classList.toggle('error', isError);
  ui.toast.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => ui.toast.classList.remove('show'), isError ? 5000 : 3200);
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

async function api(method, url, data) {
  const options = { method };
  if (data) options.body = new URLSearchParams(data);   // form-urlencoded, easy to parse in C

  let response;
  try {
    response = await fetch(url, options);
  } catch {
    ui.offline.hidden = false;
    throw new Error("Can't reach the CineVault server.");
  }
  ui.offline.hidden = true;

  let body = null;
  try { body = await response.json(); } catch { /* not JSON */ }
  if (!response.ok) {
    const error = new Error(body?.error || `The server answered with status ${response.status}.`);
    error.field = body?.field;
    throw error;
  }
  return body;
}

/* ------------------------------------------------------------------ */
/* Poster lookup (Wikipedia, no API key needed)                         */
/* ------------------------------------------------------------------ */

const STOP_WORDS = new Set(['the', 'a', 'an', 'of', 'and', 'in', 'on', 'to', 'film', 'movie']);

function normalize(text) {
  return text.normalize('NFD').replace(/[̀-ͯ]/g, '')
    .toLowerCase().replace(/[^a-z0-9]+/g, ' ').trim();
}

function keyWords(title) {
  const words = normalize(title).split(' ').filter(Boolean);
  const strong = words.filter((w) => w.length >= 3 && !STOP_WORDS.has(w));
  return strong.length ? strong : words;
}

/** Returns a poster image URL, or null if none was found (or we're offline). */
async function findPoster(title, year) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 7000);
  try {
    const params = new URLSearchParams({
      action: 'query', format: 'json', formatversion: '2', origin: '*',
      generator: 'search', gsrsearch: `${title} ${year} film`, gsrlimit: '5',
      prop: 'pageimages', piprop: 'thumbnail', pithumbsize: '500', pilicense: 'any',
    });
    const response = await fetch(`https://en.wikipedia.org/w/api.php?${params}`, { signal: controller.signal });
    if (!response.ok) return null;
    const data = await response.json();
    const words = keyWords(title);
    const pages = (data?.query?.pages ?? [])
      .filter((p) => p.thumbnail?.source)
      .sort((a, b) => a.index - b.index);
    const match = pages.find((p) => words.some((w) => normalize(p.title).split(' ').includes(w)));
    return match ? match.thumbnail.source : null;
  } catch {
    return null;
  } finally {
    clearTimeout(timer);
  }
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

/** Poster image, or a generated stand-in if there is no image or it fails to load. */
function poster(movie, className) {
  const box = el('div', { class: className });
  const useFallback = () => {
    box.classList.add('fallback');
    box.style.setProperty('--hue', hueFor(movie.title));
    box.replaceChildren(
      el('span', { class: 'fb-title' }, movie.title),
      el('span', { class: 'fb-year' }, movie.year),
    );
  };
  if (movie.image) {
    const img = el('img', {
      src: movie.image, alt: `Poster for ${movie.title}`, loading: 'lazy', referrerpolicy: 'no-referrer',
    });
    img.addEventListener('error', useFallback, { once: true });
    box.append(img);
  } else {
    useFallback();
  }
  return box;
}

function arrow() {
  const ns = 'http://www.w3.org/2000/svg';
  const svg = document.createElementNS(ns, 'svg');
  svg.setAttribute('class', 'arrow');
  svg.setAttribute('viewBox', '0 0 58 14');
  svg.setAttribute('aria-hidden', 'true');
  svg.innerHTML = '<line x1="0" y1="7" x2="49" y2="7" stroke="currentColor" stroke-width="2"/>'
    + '<path d="M47 1.5 L57 7 L47 12.5 Z" fill="currentColor"/>';
  return svg;
}

function renderSummary(stats) {
  if (!stats.total) {
    ui.summary.textContent = 'No movies yet. Add your first one with the form below.';
    return;
  }
  const top = stats.top;
  const range = stats.oldest === stats.newest
    ? `All from ${stats.oldest}.`
    : `Releases from ${stats.oldest} to ${stats.newest}.`;
  ui.summary.replaceChildren(
    el('strong', {}, plural(stats.total, 'movie')),
    ', averaging ', el('strong', {}, stats.average.toFixed(1)), ' out of 10. ',
    'Highest rated: ', el('strong', {}, top.title), ` (${top.year}) at ${top.rating.toFixed(1)}. `,
    range,
  );
}

function renderGenres(genres) {
  const names = genres.map((g) => g.name);

  // Filter dropdown: genres that exist in the collection, with counts.
  const current = state.genre;
  ui.genreFilter.replaceChildren(
    el('option', { value: '' }, 'All genres'),
    ...genres.map((g) => el('option', { value: g.name }, `${g.name} (${g.count})`)),
  );
  if (current && !names.some((n) => n.toLowerCase() === current.toLowerCase())) state.genre = '';
  ui.genreFilter.value = state.genre;

  // Suggestions in the form: the usual genres plus any custom ones already used.
  const all = [...new Set([...DEFAULT_GENRES, ...names])].sort((a, b) => a.localeCompare(b));
  ui.genreOptions.replaceChildren(...all.map((n) => el('option', { value: n })));
}

function renderReel(memory, arrivingId) {
  const nodes = memory.nodes;
  ui.nodeSize.textContent = memory.nodeSize ? `. Each node takes ${memory.nodeSize} bytes, sizeof(Movie)` : '';

  const items = [
    el('li', { class: 'head-ptr', 'aria-label': `head pointer, holds ${memory.head ?? 'NULL'}` },
      el('div', { class: 'ptr-box' },
        el('span', { class: 'ptr-name' }, 'Movie *head'),
        el('span', { class: 'addrs' }, memory.head ?? 'NULL')),
      arrow()),
  ];

  nodes.forEach((node, i) => {
    const isNew = node.id === arrivingId;
    const isRelinked = arrivingId != null && nodes[i + 1]?.id === arrivingId;
    items.push(el('li', {
      class: `node${isNew ? ' arriving' : ''}${isRelinked ? ' relinked' : ''}`,
      'aria-label': `Node ${i + 1}: ${node.title}, at ${node.addr}, next ${node.next ?? 'NULL'}`,
    },
    el('div', { class: 'frame' },
      poster(node, 'thumb'),
      el('div', { class: 'node-info' },
        el('span', { class: 'node-title' }, node.title),
        el('span', { class: 'node-sub' }, `${node.year}, id ${node.id}`),
        el('span', { class: 'addrs' },
          el('span', {}, el('span', { class: 'label' }, 'at '), node.addr),
          el('span', { class: 'next' }, el('span', { class: 'label' }, 'next '), node.next ?? 'NULL')))),
    arrow()));
  });

  items.push(el('li', { class: `null-end${arrivingId != null ? ' arriving' : ''}` }, 'NULL'));
  if (!nodes.length) items.push(el('li', { class: 'reel-empty' }, 'The list is empty, so head points to NULL.'));
  ui.track.replaceChildren(...items);

  if (arrivingId != null) {
    ui.strip.scrollTo({ left: ui.strip.scrollWidth, behavior: reducedMotion.matches ? 'auto' : 'smooth' });
  }
}

function card(movie) {
  return el('li', { class: 'card' },
    poster(movie, 'poster'),
    el('div', { class: 'meter', 'aria-hidden': 'true' },
      el('span', { style: `width:${Math.round(movie.rating * 10)}%` })),
    el('h3', { class: 'card-title' }, movie.title),
    el('p', { class: 'card-meta' },
      el('span', {}, movie.year),
      el('span', {}, movie.genre),
      el('span', { class: 'card-rating', 'aria-label': `Rated ${movie.rating.toFixed(1)} out of 10` }, movie.rating.toFixed(1))),
    el('div', { class: 'card-actions' },
      el('button', { type: 'button', class: 'link-btn', 'aria-label': `Edit ${movie.title}`, onclick: () => startEdit(movie) }, 'Edit'),
      el('button', { type: 'button', class: 'link-btn delete', 'aria-label': `Delete ${movie.title}`, onclick: () => confirmDelete(movie) }, 'Delete')));
}

function renderGrid(list) {
  ui.grid.replaceChildren(...list.movies.map(card));
  ui.count.textContent = list.count === list.total
    ? plural(list.total, 'movie')
    : `${list.count} of ${list.total} shown`;

  let emptyText = '';
  if (list.total === 0) emptyText = 'Your collection is empty. Add a movie with the form and it will appear here.';
  else if (list.count === 0) emptyText = 'No movies match. Try a different title, or choose All genres.';
  ui.empty.textContent = emptyText;
  ui.empty.hidden = !emptyText;
}

function updatePostersButton() {
  const missing = state.nodes.filter((m) => !m.image).length;
  ui.postersBtn.hidden = missing === 0;
  if (!ui.postersBtn.disabled) {
    ui.postersBtn.textContent = `Find missing posters (${missing})`;
  }
}

/* ------------------------------------------------------------------ */
/* Loading data                                                        */
/* ------------------------------------------------------------------ */

function listUrl() {
  const params = new URLSearchParams({ q: state.q, genre: state.genre, sort: state.sort });
  return `/api/movies?${params}`;
}

async function refreshGrid() {
  try {
    renderGrid(await api('GET', listUrl()));
  } catch (error) {
    toast(error.message, true);
  }
}

/** Reload everything after the collection changes. */
async function refreshAll(arrivingId = null) {
  try {
    const [stats, memory] = await Promise.all([api('GET', '/api/stats'), api('GET', '/api/list')]);
    state.nodes = memory.nodes;
    renderSummary(stats);
    renderGenres(stats.genres);
    renderReel(memory, arrivingId);
    updatePostersButton();
    renderGrid(await api('GET', listUrl()));
  } catch (error) {
    ui.summary.textContent = error.message;
  }
}

/* ------------------------------------------------------------------ */
/* Form: add and edit                                                  */
/* ------------------------------------------------------------------ */

const FIELDS = ['title', 'genre', 'year', 'rating', 'image'];

function clearErrors() {
  for (const name of FIELDS) {
    ui[name].removeAttribute('aria-invalid');
    $(`${name}-error`).textContent = '';
  }
  ui.formMessage.textContent = '';
}

function showError(field, message) {
  if (field && ui[field]) {
    ui[field].setAttribute('aria-invalid', 'true');
    $(`${field}-error`).textContent = message;
    ui[field].focus();
  } else {
    ui.formMessage.textContent = message;
  }
}

/** Same rules as validate_movie() in movies.c, so most mistakes are caught before sending. */
function validate(data) {
  if (!data.title) return ['title', 'Enter a movie title.'];
  if (!data.genre) return ['genre', 'Enter a genre, for example Action or Drama.'];
  if (!/^\d+$/.test(data.year)) return ['year', 'Enter the release year as a number, for example 2021.'];
  const year = Number(data.year);
  if (year < MIN_YEAR || year > MAX_YEAR) return ['year', `Release year must be between ${MIN_YEAR} and ${MAX_YEAR}.`];
  if (data.rating === '' || Number.isNaN(Number(data.rating))) return ['rating', 'Enter the rating as a number from 0 to 10, for example 7.6.'];
  const rating = Number(data.rating);
  if (rating < 0 || rating > 10) return ['rating', 'Rating must be between 0 and 10.'];
  if (data.image) {
    let url;
    try { url = new URL(data.image); } catch { return ['image', 'Poster link must be a full web address starting with https://.']; }
    if (url.protocol !== 'http:' && url.protocol !== 'https:') return ['image', 'Poster link must start with http:// or https://.'];
    data.image = url.href.replace(/'/g, '%27');   // encode characters the server won't store
    if (data.image.length > 511) return ['image', 'Poster link is too long (limit 511 characters).'];
  }
  return null;
}

function readForm() {
  return {
    title: ui.title.value.trim(),
    genre: ui.genre.value.trim(),
    year: ui.year.value.trim(),
    rating: ui.rating.value.trim(),
    image: ui.image.value.trim(),
  };
}

function setBusy(label) {
  state.busy = Boolean(label);
  ui.submit.disabled = state.busy;
  ui.submit.textContent = label || (state.editingId ? 'Save changes' : 'Add movie');
}

function startEdit(movie) {
  state.editingId = movie.id;
  clearErrors();
  ui.title.value = movie.title;
  ui.genre.value = movie.genre;
  ui.year.value = movie.year;
  ui.rating.value = movie.rating.toFixed(1);
  ui.image.value = movie.image;
  ui.formTitle.textContent = 'Edit movie';
  ui.submit.textContent = 'Save changes';
  ui.cancel.hidden = false;
  ui.editor.classList.add('editing');
  ui.editor.scrollIntoView({ block: 'nearest', behavior: reducedMotion.matches ? 'auto' : 'smooth' });
  ui.title.focus({ preventScroll: true });
}

function endEdit() {
  state.editingId = null;
  ui.form.reset();
  clearErrors();
  ui.formTitle.textContent = 'Add a movie';
  ui.submit.textContent = 'Add movie';
  ui.cancel.hidden = true;
  ui.editor.classList.remove('editing');
}

ui.form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (state.busy) return;
  clearErrors();

  const data = readForm();
  const problem = validate(data);
  if (problem) {
    showError(...problem);
    return;
  }

  const editingId = state.editingId;
  try {
    if (!data.image && ui.autoPoster.checked) {
      setBusy('Finding a poster…');
      data.image = (await findPoster(data.title, data.year)) || '';
    }
    setBusy(editingId ? 'Saving…' : 'Adding…');

    if (editingId) {
      const movie = await api('PUT', `/api/movies/${editingId}`, data);
      endEdit();
      toast(`Saved changes to ${movie.title}.`);
      await refreshAll();
    } else {
      const movie = await api('POST', '/api/movies', data);
      ui.form.reset();
      toast(`Added ${movie.title}.`);
      await refreshAll(movie.id);
      ui.title.focus({ preventScroll: true });
    }
  } catch (error) {
    showError(error.field, error.message);
  } finally {
    setBusy(null);
  }
});

ui.cancel.addEventListener('click', endEdit);
document.addEventListener('keydown', (event) => {
  if (event.key === 'Escape' && state.editingId && !ui.confirm.open) endEdit();
});

/* ------------------------------------------------------------------ */
/* Delete                                                              */
/* ------------------------------------------------------------------ */

function confirmDelete(movie) {
  ui.confirmTitle.textContent = `Delete ${movie.title} (${movie.year})?`;
  ui.confirm.returnValue = '';
  ui.confirm.addEventListener('close', async () => {
    if (ui.confirm.returnValue !== 'delete') return;
    try {
      await api('DELETE', `/api/movies/${movie.id}`);
      if (state.editingId === movie.id) endEdit();
      toast(`Deleted ${movie.title}.`);
      await refreshAll();
    } catch (error) {
      toast(error.message, true);
    }
  }, { once: true });
  ui.confirm.showModal();
}

/* ------------------------------------------------------------------ */
/* Find posters for movies that don't have one                         */
/* ------------------------------------------------------------------ */

ui.postersBtn.addEventListener('click', async () => {
  const missing = state.nodes.filter((m) => !m.image);
  if (!missing.length) return;

  ui.postersBtn.disabled = true;
  let found = 0;
  for (const [i, movie] of missing.entries()) {
    ui.postersBtn.textContent = `Finding posters… ${i + 1} of ${missing.length}`;
    const image = await findPoster(movie.title, movie.year);
    if (!image) continue;
    try {
      await api('PUT', `/api/movies/${movie.id}`, {
        title: movie.title, genre: movie.genre, year: movie.year, rating: movie.rating, image,
      });
      found++;
    } catch { /* skip this one */ }
  }
  ui.postersBtn.disabled = false;

  if (found) toast(`Found posters for ${found} of ${missing.length} movies.`);
  else toast('No posters found. Check your internet connection, or paste a poster link with Edit.', true);
  await refreshAll();
});

/* ------------------------------------------------------------------ */
/* Search, filter and sort (done in C on the linked list)               */
/* ------------------------------------------------------------------ */

let searchTimer;
ui.search.addEventListener('input', () => {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(() => {
    state.q = ui.search.value.trim();
    refreshGrid();
  }, 180);
});

ui.genreFilter.addEventListener('change', () => {
  state.genre = ui.genreFilter.value;
  refreshGrid();
});

ui.sort.addEventListener('change', () => {
  state.sort = ui.sort.value;
  refreshGrid();
});

/* ------------------------------------------------------------------ */
/* Start                                                               */
/* ------------------------------------------------------------------ */

ui.year.max = MAX_YEAR;
ui.year.placeholder = String(new Date().getFullYear());
refreshAll();
