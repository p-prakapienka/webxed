import createWebxedModule from './webxed.js';
import { AudioEngine } from './AudioEngine.js';
import { PatchBrowser } from './PatchBrowser.js';
import { SysexLoader } from './SysexLoader.js';
import { WebxedApi } from './WebxedApi.js';

const startButton = document.getElementById('startButton');
const sysexInput = document.getElementById('sysexInput');
const patchSelect = document.getElementById('patchSelect');
const previousButton = document.getElementById('previousButton');
const nextButton = document.getElementById('nextButton');
const noteButton = document.getElementById('noteButton');
const status = document.getElementById('status');

let audioEngine;
let patchBrowser;
let sysexLoader;

startButton.addEventListener('click', async () => {
    if (audioEngine) {
        return;
    }

    const audioContext = new AudioContext();
    const module = await createWebxedModule();
    const api = new WebxedApi(module);
    const session = api.createSession(audioContext.sampleRate);

    audioEngine = new AudioEngine(api, session, audioContext);
    patchBrowser = new PatchBrowser(api, session, patchSelect, previousButton, nextButton, status);
    sysexLoader = new SysexLoader(api, session, patchBrowser, status);

    sysexInput.disabled = false;
    noteButton.disabled = false;
    startButton.disabled = true;
    status.textContent = 'Audio ready. Load a .syx file or audition the init voice.';
    patchBrowser.refreshNavigation();
});

sysexInput.addEventListener('change', async event => {
    const [file] = event.target.files;
    if (file && sysexLoader) {
        await sysexLoader.load(file);
    }
});

patchSelect.addEventListener('change', () => patchBrowser?.select(patchSelect.selectedIndex));
previousButton.addEventListener('click', () => patchBrowser?.previous());
nextButton.addEventListener('click', () => patchBrowser?.next());

noteButton.addEventListener('pointerdown', () => audioEngine?.noteOn());
noteButton.addEventListener('pointerup', () => audioEngine?.noteOff());
noteButton.addEventListener('pointerleave', () => audioEngine?.noteOff());

window.addEventListener('keydown', event => {
    if (event.target === patchSelect || event.target === sysexInput) {
        return;
    }

    if (event.key === 'ArrowLeft') {
        patchBrowser?.previous();
    } else if (event.key === 'ArrowRight') {
        patchBrowser?.next();
    } else if (event.code === 'Space' && !event.repeat) {
        event.preventDefault();
        audioEngine?.noteOn();
    }
});

window.addEventListener('keyup', event => {
    if (event.code === 'Space') {
        audioEngine?.noteOff();
    }
});
