// Builds the DD (set time) command. The tag keeps UTC and applies the time zone itself, so it
// gets the current UTC offset and the offset changes (daylight saving time) the browser knows of.

// Must match TIME_ZONE_MAX_CHANGES in Firmware/src/domain/time_zone.h.
const MAX_OFFSET_CHANGES = 8;
const SEARCH_YEARS = 5;
const DAY_SECONDS = 86400;

export interface OffsetChange {
	at: number; // UTC unix seconds
	offsetMinutes: number; // local time minus UTC from then on
}

function offsetMinutesAt(unixSeconds: number): number {
	// getTimezoneOffset() is UTC minus local time; `|| 0` turns -0 into 0.
	return -new Date(unixSeconds * 1000).getTimezoneOffset() || 0;
}

// The browser's offset changes after `fromSeconds`, at most MAX_OFFSET_CHANGES. Scans day by day,
// then narrows each change down to the second.
export function findOffsetChanges(fromSeconds: number): OffsetChange[] {
	const changes: OffsetChange[] = [];
	const end = fromSeconds + SEARCH_YEARS * 366 * DAY_SECONDS;
	let previous = fromSeconds;
	let previousOffset = offsetMinutesAt(previous);

	for (
		let t = fromSeconds + DAY_SECONDS;
		t <= end && changes.length < MAX_OFFSET_CHANGES;
		t += DAY_SECONDS
	) {
		const offset = offsetMinutesAt(t);
		if (offset === previousOffset) {
			previous = t;
			continue;
		}
		let low = previous; // still the old offset
		let high = t; // already the new one
		while (high - low > 1) {
			const middle = Math.floor((low + high) / 2);
			if (offsetMinutesAt(middle) === previousOffset) low = middle;
			else high = middle;
		}
		changes.push({ at: high, offsetMinutes: offset });
		previous = t;
		previousOffset = offset;
	}
	return changes;
}

// DD <local time:4> <year:2> <month> <day> <weekday> in big endian, all that firmware without time
// zone support reads, then in little endian <UTC seconds:4> <ms:2> <UTC offset minutes:2> <count>
// and count changes of <UTC seconds:4> <offset minutes:2>.
export function buildSetTimeCommand(now: Date): Uint8Array<ArrayBuffer> {
	const utcMs = now.getTime();
	const utcSeconds = Math.floor(utcMs / 1000);
	const offsetMinutes = offsetMinutesAt(utcSeconds);
	const localSeconds = utcSeconds + offsetMinutes * 60;
	const changes = findOffsetChanges(utcSeconds);
	const command = new Uint8Array(19 + 6 * changes.length);
	const view = new DataView(command.buffer);

	command[0] = 0xdd;
	view.setUint32(1, localSeconds);
	view.setUint16(5, now.getFullYear());
	command[7] = now.getMonth() + 1;
	command[8] = now.getDate();
	command[9] = now.getDay();

	view.setUint32(10, utcSeconds, true);
	view.setUint16(14, utcMs % 1000, true);
	view.setInt16(16, offsetMinutes, true);
	command[18] = changes.length;
	changes.forEach((change, i) => {
		view.setUint32(19 + 6 * i, change.at, true);
		view.setInt16(19 + 6 * i + 4, change.offsetMinutes, true);
	});
	return command;
}
