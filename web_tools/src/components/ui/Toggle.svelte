<script lang="ts">
	type Props = {
		label: string;
		hint?: string;
		checked: boolean | null; // null: state unknown, shown off
		disabled?: boolean;
		onchange: (checked: boolean) => void;
	};
	let { label, hint, checked, disabled = false, onchange }: Props = $props();
</script>

<label
	class="flex min-h-14 cursor-pointer items-center justify-between gap-6 py-3 has-disabled:cursor-not-allowed"
>
	<span class="flex flex-col">
		<span class="text-[0.95rem]">{label}</span>
		{#if hint}<span class="text-sm text-muted">{hint}</span>{/if}
	</span>
	<input
		type="checkbox"
		role="switch"
		class="peer sr-only"
		checked={checked === true}
		{disabled}
		onchange={(event) => onchange(event.currentTarget.checked)}
	/>
	<span
		class="relative h-6 w-11 flex-none rounded-full border border-line-strong transition-colors peer-checked:border-accent peer-checked:bg-accent peer-focus-visible:outline-2 peer-focus-visible:outline-offset-3 peer-focus-visible:outline-accent peer-disabled:opacity-40 after:absolute after:top-1/2 after:left-1 after:size-4 after:-translate-y-1/2 after:rounded-full after:bg-soft after:transition-transform peer-checked:after:translate-x-5 peer-checked:after:bg-ink"
		aria-hidden="true"
	></span>
</label>
