class ConversionPanel {
    constructor(api, session, elements) {
        this.api = api;
        this.session = session;
        this.elements = elements;
        this.refresh();
    }

    convert() {
        if (!this.api.convert(this.session)) {
            this.elements.status.textContent = 'Convert failed.';
            this.refresh();
            return false;
        }

        this.refresh();
        const state = this.state();
        this.elements.status.textContent =
            `Converted ${state.source.name} to ${state.target.name} (Digitone algorithm ${state.target.algorithm}).`;
        return true;
    }

    save() {
        const state = this.state();
        if (!state.converted || !state.patch) {
            this.elements.status.textContent = 'Convert a patch before saving.';
            return false;
        }

        const filename = `${(state.target.name || 'converted').replace(/[^\w.-]+/g, '_')}.json`;
        const blob = new Blob([JSON.stringify(state.patch, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url;
        link.download = filename;
        document.body.append(link);
        link.click();
        link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
        this.elements.status.textContent = `Saved ${filename}.`;
        return true;
    }

    state() {
        try {
            return JSON.parse(this.api.conversionJson(this.session));
        } catch (error) {
            return { converted: false, source: { name: '', algorithm: 0 } };
        }
    }

    refresh() {
        const state = this.state();
        this.elements.sourceName.textContent = state.source?.name || '—';
        this.elements.sourceAlgorithm.textContent = state.source?.algorithm
            ? `Algorithm ${state.source.algorithm}`
            : 'Algorithm —';

        if (state.converted && state.target) {
            this.elements.targetName.textContent = state.target.name;
            this.elements.targetAlgorithm.textContent = `Algorithm ${state.target.algorithm}`;
            this.elements.digitoneNoteButton.disabled = false;
            this.elements.saveButton.disabled = false;
        } else {
            this.elements.targetName.textContent = 'Not converted';
            this.elements.targetAlgorithm.textContent = 'Algorithm —';
            this.elements.digitoneNoteButton.disabled = true;
            this.elements.saveButton.disabled = true;
        }

        this.elements.warnings.replaceChildren();
        const warnings = state.report?.warnings || [];
        if (!state.converted) {
            const item = document.createElement('li');
            item.textContent = 'Convert the selected DX patch to preview and save a Digitone approximation.';
            this.elements.warnings.append(item);
            return;
        }

        const removed = state.report?.removedOperators || [];
        if (removed.length > 0) {
            const item = document.createElement('li');
            item.textContent = `Removed DX operators ${removed.map(index => index + 1).join(', ')}.`;
            this.elements.warnings.append(item);
        }

        if (typeof state.report?.ratioApproximationError === 'number' && state.report.ratioApproximationError > 0) {
            const item = document.createElement('li');
            item.textContent = `Ratio approximation error ${state.report.ratioApproximationError.toFixed(3)}.`;
            this.elements.warnings.append(item);
        }

        warnings.forEach(warning => {
            const item = document.createElement('li');
            item.textContent = warning;
            this.elements.warnings.append(item);
        });

        if (this.elements.warnings.childElementCount === 0) {
            const item = document.createElement('li');
            item.textContent = 'No conversion warnings.';
            this.elements.warnings.append(item);
        }
    }
}
