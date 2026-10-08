import adapter from '@sveltejs/adapter-static';
import { sveltekit } from '@sveltejs/kit/vite';
import tailwindcss from '@tailwindcss/vite';
import { defineConfig } from 'vite';
import devtoolsJson from 'vite-plugin-devtools-json';

export default defineConfig({
	plugins: [
		sveltekit({
			adapter: adapter({
				pages: 'build',
				assets: 'build',
				fallback: '404.html',
				precompress: false
			}),
			paths: {
				// GitHub Pages serves the site under /<repository>; the deploy workflow sets BASE_PATH.
				base: (process.env.BASE_PATH || '') as '' | `/${string}`,
				relative: true
			}
		}),
		devtoolsJson(),
		tailwindcss()
	]
});
