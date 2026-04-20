class LogStore {
	logs: string[] = $state([]);
	private static readonly MAX_LOGS = 500;

	addLog(message: string) {
		this.logs.unshift(`[${new Date().toLocaleTimeString()}] ${message}`);
		if (this.logs.length > LogStore.MAX_LOGS) {
			this.logs.length = LogStore.MAX_LOGS;
		}
	}

	clearLogs() {
		this.logs = [];
	}
}

export const logStore = new LogStore();
