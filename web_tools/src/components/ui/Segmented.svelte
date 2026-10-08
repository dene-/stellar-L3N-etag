<script lang="ts" generics="T extends string | number">
	type Props = {
		label: string;
		options: readonly { value: T; label: string }[];
		value: T | null; // null: nothing selected yet
		disabled?: boolean;
		onchange: (value: T) => void;
	};
	let { label, options, value, disabled = false, onchange }: Props = $props();
</script>

<div
	role="radiogroup"
	aria-label={label}
	class="flex rounded-full border border-line-strong p-1 aria-disabled:opacity-45"
	aria-disabled={disabled}
>
	{#each options as option (option.value)}
		<button
			type="button"
			role="radio"
			aria-checked={option.value === value}
			{disabled}
			class="min-h-9 flex-1 rounded-full px-3 text-sm whitespace-nowrap text-soft transition-colors hover:not-disabled:text-fg aria-checked:bg-fg aria-checked:text-ink"
			onclick={() => onchange(option.value)}
		>
			{option.label}
		</button>
	{/each}
</div>
