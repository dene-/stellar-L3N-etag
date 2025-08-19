<script lang="ts">
	import '../app.css';
	import favicon from '$lib/assets/favicon.svg';

	import Navbar from '../components/Navbar.svelte';

	let { children } = $props();
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
	<title>Stellar Pro 213R-N BLE Tools</title>
</svelte:head>

<Navbar />
<div class="container mx-auto p-3">
	{@render children?.()}
</div>
