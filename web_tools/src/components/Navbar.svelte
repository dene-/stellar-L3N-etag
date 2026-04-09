<script>
	import { bleConnectionStore } from '../stores/connectionStore.svelte';
</script>

{#snippet navbarLinks()}
	<li><a href="/">Home</a></li>
	<li><a href="weather.html" target="_blank">Weather Display</a></li>
{/snippet}

<div class="drawer">
	<input id="my-drawer-3" type="checkbox" class="drawer-toggle" />
	<div class="drawer-content flex flex-col bg-base-300">
		<!-- Navbar -->
		<div class="navbar w-full container items-center mx-auto p-3">
			<div class="flex-none lg:hidden">
				<label for="my-drawer-3" aria-label="open sidebar" class="btn btn-square btn-ghost">
					<svg
						xmlns="http://www.w3.org/2000/svg"
						fill="none"
						viewBox="0 0 24 24"
						class="inline-block h-6 w-6 stroke-current"
					>
						<path
							stroke-linecap="round"
							stroke-linejoin="round"
							stroke-width="2"
							d="M4 6h16M4 12h16M4 18h16"
						></path>
					</svg>
				</label>
			</div>
			<div class="flex-1">
				<a class="text-l px-0" href="/">Hanshow Stellar Pro BLE Tools</a>
			</div>
			<div
				class="badge {bleConnectionStore.connected
					? 'badge-success'
					: bleConnectionStore.preconnected
						? 'badge-warning'
						: 'badge-neutral'}"
			>
				{#if bleConnectionStore.connected}
					<span>Connected</span>
				{:else if bleConnectionStore.preconnected}
					<span><span class="loading loading-spinner loading-xs"></span> Connecting...</span>
				{:else}
					<span>Disconnected</span>
				{/if}
			</div>
			<div class="hidden flex-none lg:block">
				<ul class="menu menu-horizontal p-0">
					<!-- Navbar menu content here -->
					{@render navbarLinks()}
				</ul>
			</div>
		</div>
	</div>
	<div class="drawer-side">
		<label for="my-drawer-3" aria-label="close sidebar" class="drawer-overlay"></label>
		<ul class="menu bg-base-200 min-h-full w-80 p-4">
			<!-- Sidebar content here -->
			{@render navbarLinks()}
		</ul>
	</div>
</div>
