<script lang="ts">
	import clockScreen from '#lib/assets/scene-clock.png';
	import dashboardScreen from '#lib/assets/scene-dashboard.png';
	import imageScreen from '#lib/assets/scene-image.png';
	import { SCENES } from '../../stores/connectionStore.svelte';
	import Icon from '../ui/Icon.svelte';

	type Props = {
		interactive: boolean; // false: a showcase, the cards are not buttons
		active: number | null;
		onselect: (scene: number) => void;
	};
	let { interactive, active, onselect }: Props = $props();

	const screens: Record<number, { src: string; caption: string }> = {
		2: { src: dashboardScreen, caption: 'Time and sensors' },
		1: { src: clockScreen, caption: 'A large clock' },
		0: { src: imageScreen, caption: 'Your last picture' },
		3: { src: imageScreen, caption: 'Your pictures in turn' }
	};

	let track: HTMLElement;

	function scroll(direction: 1 | -1) {
		const card = track.querySelector('li');
		track.scrollBy({ left: direction * ((card?.clientWidth ?? 240) + 24), behavior: 'smooth' });
	}
</script>

{#snippet screen(id: number)}
	<div class="relative mx-5 {id === 3 ? 'mt-3' : ''}">
		{#if id === 3}
			<!-- Two more frames behind the first one: a stack of pictures. -->
			<div class="absolute inset-0 translate-x-3 -translate-y-3 rounded-lg bg-paper/30"></div>
			<div class="absolute inset-0 translate-x-1.5 -translate-y-1.5 rounded-lg bg-paper/55"></div>
		{/if}
		<div class="relative rounded-lg bg-paper p-2 shadow-[0_18px_40px_-12px_rgb(0_0_0/0.7)]">
			<img
				src={screens[id].src}
				alt=""
				class="block w-full [image-rendering:pixelated]"
				width="296"
				height="128"
			/>
		</div>
	</div>
{/snippet}

<div class="flex flex-col gap-6">
	<ul
		bind:this={track}
		class="-mx-5 flex snap-x snap-mandatory gap-6 overflow-x-auto px-5 pb-2 [scrollbar-width:none] sm:mx-0 sm:px-0"
	>
		{#each SCENES as scene, index (scene.id)}
			{@const selected = interactive && active === scene.id}
			{@const cardClass = `relative flex aspect-[4/5] w-full flex-col justify-center overflow-hidden rounded-xl border bg-gradient-to-b from-raised to-ink text-left transition-colors ${selected ? 'border-accent' : 'border-line'}`}
			<li class="w-60 flex-none snap-start sm:w-64">
				{#snippet body()}
					{@render screen(scene.id)}
					<div class="absolute inset-x-0 bottom-0 flex items-end justify-between gap-3 p-5">
						<div>
							<div class="text-lg text-fg">{scene.name}</div>
							<div class="text-sm text-muted">{screens[scene.id].caption}</div>
						</div>
						<span class="eyebrow whitespace-nowrap {selected ? 'text-accent!' : ''}">
							{selected ? 'On screen' : String(index + 1).padStart(2, '0')}
						</span>
					</div>
				{/snippet}
				{#if interactive}
					<button
						type="button"
						class="{cardClass} cursor-pointer hover:border-line-strong"
						aria-pressed={selected}
						onclick={() => onselect(scene.id)}>{@render body()}</button
					>
				{:else}
					<div class={cardClass}>{@render body()}</div>
				{/if}
			</li>
		{/each}
	</ul>
	<div class="flex justify-end gap-4">
		<button class="icon-btn" aria-label="Previous screens" onclick={() => scroll(-1)}>
			<Icon name="chevron-left" />
		</button>
		<button class="icon-btn icon-btn-accent" aria-label="Next screens" onclick={() => scroll(1)}>
			<Icon name="chevron-right" />
		</button>
	</div>
</div>
