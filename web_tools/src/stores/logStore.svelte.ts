class LogStore {
	// Newest first.
	logs: string[] = $state([]);
	// Messages added since the page loaded, including ones dropped from logs.
	total = $state(0);
	private static readonly MAX_LOGS = 500;

	addLog(message: string) {
		this.logs.unshift(`[${new Date().toLocaleTimeString()}] ${message}`);
		this.total++;
		if (this.logs.length > LogStore.MAX_LOGS) {
			this.logs.length = LogStore.MAX_LOGS;
		}
	}

	clearLogs() {
		this.logs = [];
	}
}

export const logStore = new LogStore();
