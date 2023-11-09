import { afterAll, bench, suite } from 'vitest';
import { GCProfiler, setFlagsFromString } from 'node:v8';
import { runInNewContext } from 'node:vm';
// import { PerformanceObserver } from 'node:perf_hooks';

// // via https://stackoverflow.com/a/75007985/151464
// setFlagsFromString('--expose-gc');
// const gc = global.gc!;
// const gc = runInNewContext("gc");
if (!global.gc) {
	console.warn("can't request GC from V8, accuracy of measurements may be impacted")
}

function runGC() {
	// these are the defaults, but to be explicit
	// see: https://github.com/nodejs/node/blob/5421e15bdc6122420d90ec462c3622b57d59c9d6/deps/v8/src/extensions/gc-extension.cc#L40-L42
	// gc({
	// 	'type': 'full',
	// 	'execution': 'sync',
	// })
	// gc()
	global.gc?.call(undefined);
}

suite('strings and things', () => {
	membench('hi', () => {
		var s = '';

		for (let i = 0; i < 500; i++) {
			s += 'a';
		}
	})
	membench('hi2', () => {
		var s = '';

		for (let i = 0; i < 1000; i++) {
			s += 'a';
		}
	})
})

afterAll(() => {
	console.log("hi there");
})

function membench(name: string, fn: () => void) {
	bench(name, fn,
		{
			setup: (task, mode) => {
				if (mode == 'warmup') return;

				const profiler = new GCProfiler();

				task.opts.beforeAll = () => {
					runGC()
					// setFlagsFromString('--trace-gc')
					profiler.start()
				}
				task.opts.afterAll = async function () {
					runGC()
					// setFlagsFromString('--notrace-gc')
					const profile = profiler.stop();

					let heapUsage = 0;
					for (const stat of profile.statistics) {
						heapUsage += stat.beforeGC.heapStatistics.usedHeapSize - stat.afterGC.heapStatistics.usedHeapSize
					}

					console.log(`\`${name}\` usage: ${heapUsage} bytes (${(heapUsage / 1024 / 1024).toFixed(1)} MB) across ${this.runs} runs (${(heapUsage / this.runs).toFixed(2)} bytes per run)`)
					// console.log(profile.statistics[0], `(+ ${ profile.statistics.length - 1 } more)`)
					// console.log(profile.statistics[0]?.afterGC.heapSpaceStatistics)
				}
			}
		})
}



// profile.statistics[0]
// ->
// {
//   gcType: 'Scavenge',
//   beforeGC: {
//     heapStatistics: {
//       totalHeapSize: 27205632,
//       totalHeapSizeExecutable: 786432,
//       totalPhysicalSize: 27168768,
//       totalAvailableSize: 4326618352,
//       totalGlobalHandlesSize: 16384,
//       usedGlobalHandlesSize: 12608,
//       usedHeapSize: 16561736,
//       heapSizeLimit: 4345298944,
//       mallocedMemory: 311424,
//       externalMemory: 2096301,
//       peakMallocedMemory: 5445632
//     },
//     heapSpaceStatistics: [
//       ... (below) ...
//     ]
//   },
//   cost: 123.474,
//   afterGC: {
//     heapStatistics: {
//       totalHeapSize: 27205632,
//       totalHeapSizeExecutable: 786432,
//       totalPhysicalSize: 27168768,
//       totalAvailableSize: 4336619480,
//       totalGlobalHandlesSize: 16384,
//       usedGlobalHandlesSize: 12608,
//       usedHeapSize: 8319472,
//       heapSizeLimit: 4345298944,
//       mallocedMemory: 311424,
//       externalMemory: 2086037,
//       peakMallocedMemory: 5445632
//     },
//     heapSpaceStatistics: [
//       ... (below) ...
//     ]
//   }
// } (+ 313 more)



// profile.statistics[0]?.afterGC.heapSpaceStatistics
// ->
// [
// 	{
// 		spaceName: 'read_only_space',
// 		spaceSize: 0,
// 		spaceUsedSize: 0,
// 		spaceAvailableSize: 0,
// 		physicalSpaceSize: 0
// 	},
// 	{
// 		spaceName: 'new_space',
// 		spaceSize: 16777216,
// 		spaceUsedSize: 7104,
// 		spaceAvailableSize: 8239936,
// 		physicalSpaceSize: 16777216
// 	},
// 	{
// 		spaceName: 'old_space',
// 		spaceSize: 8650752,
// 		spaceUsedSize: 6631744,
// 		spaceAvailableSize: 1870712,
// 		physicalSpaceSize: 8650752
// 	},
// 	{
// 		spaceName: 'code_space',
// 		spaceSize: 786432,
// 		spaceUsedSize: 710224,
// 		spaceAvailableSize: 26912,
// 		physicalSpaceSize: 749568
// 	},
// 	{
// 		spaceName: 'shared_space',
// 		spaceSize: 0,
// 		spaceUsedSize: 0,
// 		spaceAvailableSize: 0,
// 		physicalSpaceSize: 0
// 	},
// 	{
// 		spaceName: 'new_large_object_space',
// 		spaceSize: 0,
// 		spaceUsedSize: 0,
// 		spaceAvailableSize: 8388608,
// 		physicalSpaceSize: 0
// 	},
// 	{
// 		spaceName: 'large_object_space',
// 		spaceSize: 991232,
// 		spaceUsedSize: 970400,
// 		spaceAvailableSize: 0,
// 		physicalSpaceSize: 991232
// 	},
// 	{
// 		spaceName: 'code_large_object_space',
// 		spaceSize: 0,
// 		spaceUsedSize: 0,
// 		spaceAvailableSize: 0,
// 		physicalSpaceSize: 0
// 	},
// 	{
// 		spaceName: 'shared_large_object_space',
// 		spaceSize: 0,
// 		spaceUsedSize: 0,
// 		spaceAvailableSize: 0,
// 		physicalSpaceSize: 0
// 	}
// ]
