<script lang="ts">
	import { logStore } from '../stores/logStore.svelte';
	import Icon from './ui/Icon.svelte';

	let { onlog }: { onlog: () => void } = $props();

	const REPOSITORY = 'https://github.com/dene-/stellar-L3N-etag';
</script>

<footer class="mx-auto flex w-full max-w-7xl items-center gap-6 px-5 pt-4 pb-8 sm:px-10">
	<a
		href={REPOSITORY}
		class="grid size-11 place-items-center text-muted hover:text-fg"
		target="_blank"
		rel="noreferrer"
		aria-label="Source code on GitHub"
	>
		<Icon name="github" />
	</a>
	<a
		href="{REPOSITORY}#readme"
		class="grid size-11 place-items-center text-muted hover:text-fg"
		target="_blank"
		rel="noreferrer"
		aria-label="Guide"
	>
		<Icon name="book" />
	</a>
	<!-- The latest log line: feedback for every action without opening the log. -->
	<button
		class="ml-auto min-h-11 min-w-0 truncate text-right font-mono text-xs text-muted hover:text-soft"
		aria-live="polite"
		onclick={onlog}
	>
		{logStore.logs[0] ?? ''}
	</button>
</footer>
