"use strict";

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

$(function() {
    const spsc_pattern = { pattern: {
        path: {
            d: 'M 0 0 L 10 10 M 9 -1 L 11 1 M -1 9 L 1 11',
            strokeWidth: 4
        },
        width: 10,
        height: 10,
        opacity: 1
    }};

    const settings = {
     "boost::lockfree::spsc_queue": [$.extend(true, {pattern: {color: '#8E44AD'}}, spsc_pattern),  0],
   "moodycamel::ReaderWriterQueue": [$.extend(true, {pattern: {color: '#BA4A00'}}, spsc_pattern),  1],
                "pthread_spinlock": ['#58D68D',  2],
                      "std::mutex": ['#239B56',  3],
                 "tbb::spin_mutex": ['#3498DB',  4],
   "tbb::concurrent_bounded_queue": ['#9ACCED',  5],
          "boost::lockfree::queue": ['#AA73C2',  6],
     "moodycamel::ConcurrentQueue": ['#BA4A00',  7],
     "xenium::michael_scott_queue": ['#73C6B6',  8],
         "xenium::ramalhete_queue": ['#45B39D',  9],
    "xenium::vyukov_bounded_queue": ['#16A085', 10],
                     "AtomicQueue": ['#FFFF00', 11],
                    "AtomicQueueB": ['#FFFF40', 12],
                    "AtomicQueue2": ['#FFFF80', 13],
                   "AtomicQueueB2": ['#FFFFBF', 14],
             "OptimistAtomicQueue": ['#FF0000', 15],
            "OptimistAtomicQueueB": ['#FF4040', 16],
            "OptimistAtomicQueue2": ['#FF8080', 17],
           "OptimistAtomicQueueB2": ['#FFBFBF', 18]
    };

    // function getSeriesColor(s) {
    //     const c = s[0];
    //     return c?.pattern?.color ?? c;
    // }

    function fmt(v) {
        return Highcharts.numberFormat(v, 0);
    }

    function plot_scalability(div_id, results, max_lin, max_log) {
        const modes = [
            {
                yAxis: { type: 'linear', title: { text: 'throughput, msg/sec (linear scale)' }, max: max_lin, min: 0 },
                subtitle: {text: 'click on the chart background to switch to logarithmic scale'},
            },
            {
                yAxis: { type: 'logarithmic', title: { text: 'throughput, msg/sec (logarithmic scale)' }, max: max_log, min: 100e3 },
                subtitle: {text: 'click on the chart background to switch to linear scale'},
            }
        ];
        let mode = 0;

        const series = [];
        for(const [name, stats] of Object.entries(results)) {
            const s = settings[name];
            series.push({
                name: name,
                color: s[0],
                index: s[1],
                type: "column",
                data: stats.map(a => [a[0], a[3]]),
                atomic_queue_stats: stats,
            });
            series.push({
                name: `${name} StdDev`,
                type: 'errorbar',
                index: s[1],
                data: stats.map(a => [a[0], a[3] - a[4], a[3] + a[4]]),
            });
        }

        const tooltips = [];
        const tooltip_formatter = function() {
            const n_threads = this.x;
            const tooltip = tooltips[n_threads];
            if(tooltip)
                return tooltip;

            const data = [];
            for(const p of this.points) {
                const [n_threads, min, max, mean, stdev] = p.series.options.atomic_queue_stats[p.point.index].map(fmt);
                data[p.series.options.index] = {
                    name: p.series.name,
                    color: p.series.color,
                    min: min,
                    max: max,
                    mean: mean,
                    stdev: stdev,
                };
            }
            const tbody = data.map((d) => `<tr><td style="color: ${d.color}">${d.name}: </td><td><strong>${d.mean}</strong></td><td><strong>${d.stdev}</strong></td><td>${d.min}</td><td>${d.max}</td></tr>`).join('\n')
            return tooltips[n_threads] = `
                <span class="tooltip_scalability_title">${n_threads} producers, ${n_threads} consumers</span>
                <table class="tooltip_scalability">
                <tr><th></th><th>mean</th><th>stdev</th><th>min</th><th>max</th></tr>
                ${tbody}
                </table>
                `;
        };

        Highcharts.chart(div_id, {
            series: series,
            chart: {
                events: {
                    click: function() {
                        const m = modes[mode ^= 1];
                        this.yAxis[0].update(m.yAxis);
                        this.subtitle.update(m.subtitle);
                    }
                }
            },
            subtitle: modes[0].subtitle,
            yAxis: modes[0].yAxis,
            xAxis: {
                title: { text: 'number of producers, number of consumers' },
                tickInterval: 1
            },
            tooltip: { formatter: tooltip_formatter },
        });
    }

    const latencyChartOptions = {
        xAxis: {
            categories: Object.keys(settings),
            labels: { rotation: 0 }
        },
        yAxis: {
            min: 0,
            max: 1000,
            tickInterval: 100,
            title: { text: 'latency, nanoseconds/round-trip' }
        },
        tooltip: {
            formatter: function() {
                const [min, max, mean, stdev] = this.series.options.atomic_queue_stats.map(fmt);
                return `<strong>mean: ${mean} stdev: ${stdev}</strong> min: ${min} max: ${max}<br/>`;
            }
        },
    };
    function plot_latency(div_id, results) {
        function chartData(chartType) {
            if (chartType === 'boxplot') {
                const series = [];
                for(const [name, s] of Object.entries(settings)) {
                    let stats = results[name];
                    if(!stats)
                        continue;
                    const [color, index] = s;
                    const [min, max, mean, stdev] = stats;
                    const q1 = mean - stdev;
                    const q3 = mean + stdev;
                    series.push({
                        name: name,
                        color: color,
                        medianColor: color,
                        type: 'boxplot',
                        data: [[index, min, q1, mean, q3, max]],
                        atomic_queue_stats: stats,
                    });
                }
                return {
                    series: series,
                    subtitle: { text: 'click on the chart background to switch to bar chart view' },
                    chart: {
                        inverted: true,
                        events: { click: createChart.bind(null, "bar") }
                    },
                };
            } else {
                const series = [];
                for(const [name, s] of Object.entries(settings)) {
                    let stats = results[name];
                    if(!stats)
                        continue;
                    const [color, index] = s;
                    const mean = stats[2];
                    const stdev = stats[3];
                    series.push({
                        name: name,
                        color: color,
                        type: 'bar',
                        data: [[index, mean]],
                        atomic_queue_stats: stats,
                    });
                    series.push({
                        name: `${s.name} StdDev`,
                        type: 'errorbar',
                        data: [[index, mean - stdev, mean + stdev]],
                    });
                }
                return {
                    series: series,
                    subtitle: { text: 'click on the chart background to switch to boxplot view' },
                    chart: { events: { click: createChart.bind(null, "boxplot") } },
                };
            }
        };

        function createChart(chartType) {
            Highcharts.chart(div_id, $.extend(true, chartData(chartType), latencyChartOptions));
        };
        createChart("bar");
    };

    plot_scalability('scalability-9900KS-5GHz', scalability_9900KS, 60e6, 1000e6);
    plot_scalability('scalability-ryzen-5825u', scalability_ryzen_5825u, 40e6, 1000e6);
    plot_scalability('scalability-xeon-gold-6132', scalability_xeon_gold_6132, 15e6, 300e6);
    plot_scalability('scalability-ryzen-5950x', scalability_ryzen_5950x, 20e6, 500e6);
    plot_latency('latency-9900KS-5GHz', latency_9900KS);
    plot_latency('latency-ryzen-5825u', latency_ryzen_5825u);
    plot_latency('latency-xeon-gold-6132', latency_xeon_gold_6132);
    plot_latency('latency-ryzen-5950x', latency_ryzen_5950x);

    $(".view-toggle")
        .html((index, html) => `<svg class="arrow-down-circle" viewBox="0 0 16 16"><path fill-rule="evenodd" d="M1 8a7 7 0 1 0 14 0A7 7 0 0 0 1 8zm15 0A8 8 0 1 1 0 8a8 8 0 0 1 16 0zM8.5 4.5a.5.5 0 0 0-1 0v5.793L5.354 8.146a.5.5 0 1 0-.708.708l3 3a.5.5 0 0 0 .708 0l3-3a.5.5 0 0 0-.708-.708L8.5 10.293V4.5z"/></svg> ${html}`)
        .on("click", function() {
            const toggle = $(this);
            toggle.next().slideToggle();
            toggle.children("svg.arrow-down-circle").each(function() {
                $(this).animate(
                    { hidden: this.hidden ^ 1 },
                    { step: function(f) { $(this).css("transform", `rotate(${-90 * f}deg)`); } }
                );
            });
        });

    $("body").css("visibility", "visible");
});
