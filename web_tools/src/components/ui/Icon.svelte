<script lang="ts">
	// Line icons on a 24 px grid, drawn with currentColor.
	const paths = {
		menu: 'M4 9h16M8 15h12',
		close: 'M6 6l12 12M18 6 6 18',
		'chevron-left': 'm14.5 6-6 6 6 6',
		'chevron-right': 'm9.5 6 6 6-6 6',
		'arrow-up': 'M12 19V5m-6 6 6-6 6 6',
		'arrow-down': 'M12 5v14m-6-6 6 6 6-6',
		'arrow-left': 'M19 12H5m6-6-6 6 6 6',
		'arrow-right': 'M5 12h14m-6-6 6 6-6 6',
		'rotate-left': 'M4 5v5h5M4.6 14a8 8 0 1 0 1.9-8.3L4 10',
		'rotate-right': 'M20 5v5h-5M19.4 14a8 8 0 1 1-1.9-8.3L20 10',
		center: 'M12 4v4m0 8v4M4 12h4m8 0h4M12 12h.01',
		bluetooth: 'm7 7 10 10-5 4V3l5 4L7 17',
		upload: 'M12 16V4m-5 5 5-5 5 5M5 20h14',
		github:
			'M9 19c-4.3 1.4-4.3-2.5-6-3m12 5v-3.5c0-1 .1-1.4-.5-2 2.8-.3 5.5-1.4 5.5-6a4.6 4.6 0 0 0-1.3-3.2 4.2 4.2 0 0 0-.1-3.2s-1.1-.3-3.5 1.3a12.3 12.3 0 0 0-6.2 0C6.5 2.8 5.4 3.1 5.4 3.1a4.2 4.2 0 0 0-.1 3.2A4.6 4.6 0 0 0 4 9.5c0 4.6 2.7 5.7 5.5 6-.6.6-.6 1.2-.5 2V21',
		book: 'M4 5.5A1.5 1.5 0 0 1 5.5 4H11v16H5.5A1.5 1.5 0 0 1 4 18.5zM20 5.5A1.5 1.5 0 0 0 18.5 4H13v16h5.5a1.5 1.5 0 0 0 1.5-1.5z'
	} as const;

	type Props = { name: keyof typeof paths; size?: number; class?: string };
	let { name, size = 20, class: className = '' }: Props = $props();
</script>

<svg
	class={className}
	width={size}
	height={size}
	viewBox="0 0 24 24"
	fill="none"
	stroke="currentColor"
	stroke-width="1.5"
	stroke-linecap="round"
	stroke-linejoin="round"
	aria-hidden="true"><path d={paths[name]} /></svg
>
