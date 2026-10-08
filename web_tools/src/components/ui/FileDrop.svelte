<script lang="ts">
	import type { Snippet } from 'svelte';

	type Props = {
		accept: string;
		multiple?: boolean;
		disabled?: boolean;
		class?: string;
		onfiles: (files: File[]) => void;
		children: Snippet<[{ dragging: boolean }]>;
	};
	let {
		accept,
		multiple = false,
		disabled = false,
		class: className = '',
		onfiles,
		children
	}: Props = $props();

	let dragging = $state(false);

	function take(list: FileList | null | undefined) {
		const files = Array.from(list ?? []);
		if (files.length) onfiles(multiple ? files : files.slice(0, 1));
	}
</script>

<label
	class="flex cursor-pointer flex-col items-center justify-center rounded-2xl border border-dashed text-center transition-colors {dragging
		? 'border-accent bg-accent/5'
		: 'border-line-strong hover:border-muted'} has-disabled:cursor-not-allowed has-disabled:opacity-45 has-focus-visible:border-accent {className}"
	ondragover={(event) => {
		event.preventDefault();
		if (!disabled) dragging = true;
	}}
	ondragleave={() => (dragging = false)}
	ondrop={(event) => {
		event.preventDefault();
		dragging = false;
		if (!disabled) take(event.dataTransfer?.files);
	}}
>
	<input
		type="file"
		class="sr-only"
		{accept}
		{multiple}
		{disabled}
		onchange={(event) => {
			take(event.currentTarget.files);
			event.currentTarget.value = ''; // picking the same file again fires change again
		}}
	/>
	{@render children({ dragging })}
</label>
