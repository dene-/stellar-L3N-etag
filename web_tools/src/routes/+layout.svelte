<script lang="ts">
	import '@fontsource-variable/jost';
	import '../app.css';
	import favicon from '#lib/assets/favicon.svg';

	let { children } = $props();
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
	<title>Stellar e-paper tag tools</title>
	<meta
		name="description"
		content="Set up Hanshow Stellar e-paper tags from the browser over Bluetooth."
	/>
</svelte:head>

{@render children()}
