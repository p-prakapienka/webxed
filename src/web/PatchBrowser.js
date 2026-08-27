export class PatchBrowser {
    constructor(api, session, patchSelect, previousButton, nextButton, status) {
        this.api = api;
        this.session = session;
        this.patchSelect = patchSelect;
        this.previousButton = previousButton;
        this.nextButton = nextButton;
        this.status = status;
    }

    load(count) {
        this.patchSelect.replaceChildren();
        for (let index = 0; index < count; index += 1) {
            const option = document.createElement('option');
            option.value = String(index);
            option.textContent = this.api.patchName(this.session, index) || `Patch ${index + 1}`;
            this.patchSelect.append(option);
        }
        this.select(0);
    }

    select(index) {
        if (index < 0 || index >= this.patchSelect.options.length) {
            return false;
        }
        if (!this.api.selectPatch(this.session, index)) {
            return false;
        }

        this.patchSelect.selectedIndex = index;
        this.refreshNavigation();
        this.status.textContent = `Selected ${index + 1}/${this.patchSelect.options.length}: ${this.patchSelect.options[index].text}`;
        return true;
    }

    previous() {
        this.select(this.patchSelect.selectedIndex - 1);
    }

    next() {
        this.select(this.patchSelect.selectedIndex + 1);
    }

    refreshNavigation() {
        const count = this.patchSelect.options.length;
        const enabled = count > 0;
        this.patchSelect.disabled = !enabled;
        this.previousButton.disabled = !enabled || this.patchSelect.selectedIndex <= 0;
        this.nextButton.disabled = !enabled || this.patchSelect.selectedIndex >= count - 1;
    }
}
