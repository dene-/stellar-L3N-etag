<script lang="ts">
	import '../app.css';
	import favicon from '$lib/assets/favicon.svg';

	import Navbar from '../components/Navbar.svelte';

	let { children } = $props();
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
	<title>Hanshow Stellar Pro BLE Tools</title>
</svelte:head>

<Navbar />
<div class="container mx-auto flex flex-col p-3 h-full">
	{@render children?.()}
</div>
