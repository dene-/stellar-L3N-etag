class LogStore {
  logs: string[] = $state([]);

  addLog(message: string) {
    this.logs.unshift(`[${new Date().toLocaleTimeString()}] ${message}`);
  }

  clearLogs() {
    this.logs = [];
  }
}

export const logStore = new LogStore();
