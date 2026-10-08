<script lang="ts">
	import BleUpdateCard from './BleUpdateCard.svelte';
	import SerialFlashCard from './SerialFlashCard.svelte';
</script>

<div class="flex flex-col gap-12">
	<div class="flex flex-col gap-4">
		<p class="eyebrow">Firmware</p>
		<h1 class="display text-4xl sm:text-5xl">Keep the tag up to date.</h1>
		<p class="max-w-xl text-soft">
			Get the <code class="font-mono text-sm text-fg">.bin</code> file from the
			<a
				class="text-accent underline-offset-4 hover:underline"
				href="https://github.com/dene-/stellar-L3N-etag/releases/latest"
				target="_blank"
				rel="noreferrer">latest release</a
			>. Tags running this firmware update over Bluetooth; a tag still on its original firmware
			needs one install over USB first.
		</p>
	</div>
	<div class="grid gap-6 lg:grid-cols-2">
		<BleUpdateCard />
		<SerialFlashCard />
	</div>
</div>
