const startButton = document.getElementById('startButton');
const sysexInput = document.getElementById('sysexInput');
const patchSelect = document.getElementById('patchSelect');
const previousButton = document.getElementById('previousButton');
const nextButton = document.getElementById('nextButton');
const convertButton = document.getElementById('convertButton');
const editButton = document.getElementById('editButton');
const saveButton = document.getElementById('saveButton');
const dxNoteButton = document.getElementById('dxNoteButton');
const digitoneNoteButton = document.getElementById('digitoneNoteButton');
const status = document.getElementById('status');

let audioEngine;
let patchBrowser;
let sysexLoader;
let conversionPanel;
let digitoneEditor;

function envelopeFields(prefix) {
    return {
        attack: document.getElementById(`${prefix}Attack`),
        decay: document.getElementById(`${prefix}Decay`),
        endLevel: document.getElementById(`${prefix}EndLevel`),
        level: document.getElementById(`${prefix}Level`),
        delay: document.getElementById(`${prefix}Delay`),
        triggered: document.getElementById(`${prefix}Triggered`),
        reset: document.getElementById(`${prefix}Reset`)
    };
}

function isEditableTarget(target) {
    const tag = target && target.tagName;
    return tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA';
}

startButton.addEventListener('click', async () => {
    if (audioEngine) {
        return;
    }

    const audioContext = new AudioContext();
    const module = await createWebxedModule();
    const api = new WebxedApi(module);
    const session = api.createSession(audioContext.sampleRate);

    audioEngine = new AudioEngine(api, session, audioContext);
    conversionPanel = new ConversionPanel(api, session, {
        sourceName: document.getElementById('sourceName'),
        sourceAlgorithm: document.getElementById('sourceAlgorithm'),
        targetName: document.getElementById('targetName'),
        targetAlgorithm: document.getElementById('targetAlgorithm'),
        warnings: document.getElementById('warnings'),
        digitoneNoteButton,
        saveButton,
        status
    });
    digitoneEditor = new DigitoneEditor(api, session, conversionPanel, {
        panel: document.getElementById('editorPanel'),
        form: document.getElementById('editorForm'),
        editButton,
        status,
        name: document.getElementById('editName'),
        algorithm: document.getElementById('editAlgorithm'),
        ratioC: document.getElementById('editRatioC'),
        ratioA: document.getElementById('editRatioA'),
        ratioB1: document.getElementById('editRatioB1'),
        ratioB2: document.getElementById('editRatioB2'),
        harmonic: document.getElementById('editHarmonic'),
        detune: document.getElementById('editDetune'),
        feedback: document.getElementById('editFeedback'),
        mix: document.getElementById('editMix'),
        envelopeA: envelopeFields('editEnvA'),
        envelopeB: envelopeFields('editEnvB')
    });
    patchBrowser = new PatchBrowser(
        api,
        session,
        patchSelect,
        previousButton,
        nextButton,
        status,
        () => {
            conversionPanel.refresh();
            digitoneEditor.refresh();
        }
    );
    sysexLoader = new SysexLoader(api, session, patchBrowser, status);

    sysexInput.disabled = false;
    dxNoteButton.disabled = false;
    convertButton.disabled = false;
    startButton.disabled = true;
    status.textContent = 'Audio ready. Load a .syx file or convert the DX init voice.';
    patchBrowser.refreshNavigation();
    conversionPanel.refresh();
    digitoneEditor.refresh();
});

sysexInput.addEventListener('change', async event => {
    const [file] = event.target.files;
    if (file && sysexLoader) {
        await sysexLoader.load(file);
        conversionPanel?.refresh();
        digitoneEditor?.refresh();
    }
});

patchSelect.addEventListener('change', () => patchBrowser?.select(patchSelect.selectedIndex));
previousButton.addEventListener('click', () => patchBrowser?.previous());
nextButton.addEventListener('click', () => patchBrowser?.next());
convertButton.addEventListener('click', () => {
    if (conversionPanel?.convert()) {
        digitoneEditor?.refresh();
    }
});
saveButton.addEventListener('click', () => conversionPanel?.save());

dxNoteButton.addEventListener('pointerdown', () => {
    if (audioEngine?.selectPreviewEngine(0)) {
        audioEngine.noteOn();
    }
});
dxNoteButton.addEventListener('pointerup', () => audioEngine?.noteOff());
dxNoteButton.addEventListener('pointerleave', () => audioEngine?.noteOff());

digitoneNoteButton.addEventListener('pointerdown', () => {
    if (audioEngine?.selectPreviewEngine(1)) {
        audioEngine.noteOn();
    }
});
digitoneNoteButton.addEventListener('pointerup', () => audioEngine?.noteOff());
digitoneNoteButton.addEventListener('pointerleave', () => audioEngine?.noteOff());

window.addEventListener('keydown', event => {
    if (isEditableTarget(event.target) && event.target !== patchSelect) {
        return;
    }

    const key = event.key.toLowerCase();
    if (event.key === 'ArrowLeft') {
        patchBrowser?.previous();
    } else if (event.key === 'ArrowRight') {
        patchBrowser?.next();
    } else if (event.code === 'Space' && !event.repeat) {
        event.preventDefault();
        audioEngine?.selectPreviewEngine(0);
        audioEngine?.noteOn();
    } else if (key === 'n' && !event.repeat) {
        if (audioEngine?.selectPreviewEngine(1)) {
            audioEngine.noteOn();
        }
    } else if (key === 'd' && !event.repeat) {
        if (audioEngine?.selectPreviewEngine(0)) {
            audioEngine.noteOn();
        }
    } else if (key === 'c' && !event.repeat) {
        if (conversionPanel?.convert()) {
            digitoneEditor?.refresh();
        }
    } else if (key === 'e' && !event.repeat) {
        digitoneEditor?.toggle();
    } else if (key === 's' && !event.repeat) {
        event.preventDefault();
        conversionPanel?.save();
    }
});

window.addEventListener('keyup', event => {
    if (isEditableTarget(event.target) && event.target !== patchSelect) {
        return;
    }

    const key = event.key.toLowerCase();
    if (event.code === 'Space' || key === 'n' || key === 'd') {
        audioEngine?.noteOff();
    }
});
