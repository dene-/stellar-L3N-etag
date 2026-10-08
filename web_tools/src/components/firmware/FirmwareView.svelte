<script lang="ts">
	import BleUpdateCard from './BleUpdateCard.svelte';
	import SerialFlashCard from './SerialFlashCard.svelte';
</script>

<div class="flex flex-col gap-12">
	<div class="flex flex-col gap-4">
		<p class="eyebrow">Firmware</p>
		<h1 class="display text-4xl sm:text-5xl">Install and update</h1>
		<p class="max-w-xl text-soft">
			Pick a release, or a <code class="font-mono text-sm text-fg">.bin</code> file of your own.
			Tags running this firmware update over Bluetooth; a tag still on its original firmware needs
			one install over USB first. Release notes are on
			<a
				class="text-accent underline-offset-4 hover:underline"
				href="https://github.com/dene-/stellar-L3N-etag/releases"
				target="_blank"
				rel="noreferrer">GitHub</a
			>.
		</p>
	</div>
	<div class="grid gap-6 lg:grid-cols-2">
		<BleUpdateCard />
		<SerialFlashCard />
	</div>
</div>
