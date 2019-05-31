"use strict";

$(function() {
    Highcharts.setOptions({
	lang: {
  	    thousandsSep: ','
        }
    });

    function results_to_series(results) {
        return Array.from(Object.entries(results)).map(entry => {
            return {
                name: entry[0],
                data: Array.from(Object.entries(entry[1])).map(xy => { return [parseInt(xy[0]), xy[1]]; })
            };
        });
    }

    function plot(div_id, series, title_suffix) {
        Highcharts.chart(div_id, {
            chart: { type: 'column' },
            title: { text: 'Scalability on ' + title_suffix },

            xAxis: {
                title: { text: 'number of producers, number of consumers' },
                tickInterval: 1
            },
            yAxis: {
                title: { text: 'throughput, msg/sec' }
            },

            legend: {
                layout: 'horizontal',
                align: 'center',
                verticalAlign: 'bottom'
            },

            plotOptions: {
            },

            tooltip: {
                followPointer: true,
                useHTML: true,
                shared: true,
                headerFormat: '<span style="font-weight: bold; font-size: 1.2em;">{point.key} producers, {point.key} consumers</span><table>',
                pointFormat: '<tr><td style="color: {series.color}">{series.name}: </td>' +'<td style="text-align: right"><b>{point.y} msg/sec</b></td></tr>',
                footerFormat: '</table>'
            },

            credits: { enabled: false },

            series: series
        });
    }

    // TODO: load these from files.
    const results_7700k = {"AtomicQueue":{"1":27473115.0,"2":13780199.0,"3":13532478.0},"AtomicQueue2":{"1":25092312.0,"2":14274779.0,"3":12566061.0},"BlockingAtomicQueue":{"1":117895405.0,"2":43575992.0,"3":36987120.0},"BlockingAtomicQueue2":{"1":98733131.0,"2":38478447.0,"3":37995297.0},"boost::lockfree::queue":{"1":10205501.0,"2":7970549.0,"3":7920727.0},"pthread_spinlock":{"1":41729933.0,"2":15577961.0,"3":14945822.0},"tbb::concurrent_bounded_queue":{"1":15349404.0,"2":15966472.0,"3":14038820.0},"tbb::speculative_spin_mutex":{"1":53445745.0,"2":39245530.0,"3":31189300.0},"tbb::spin_mutex":{"1":85232248.0,"2":54468218.0,"3":38036453.0}};
    const results_xeon_gold_6132 = {"AtomicQueue":{"1":9759971.0,"2":5551276.0,"3":3937683.0,"4":3492101.0,"5":2650348.0,"6":2492281.0,"7":1304372.0,"8":1083301.0,"9":1013039.0,"10":1071124.0,"11":1040391.0,"12":1079620.0,"13":1256532.0},"AtomicQueue2":{"1":8411912.0,"2":4776622.0,"3":3452154.0,"4":3139269.0,"5":2855631.0,"6":2403095.0,"7":1368201.0,"8":1061715.0,"9":1066555.0,"10":1055948.0,"11":1118051.0,"12":1068713.0,"13":1078537.0},"BlockingAtomicQueue":{"1":68666934.0,"2":17451686.0,"3":17462525.0,"4":20015414.0,"5":21160763.0,"6":21323176.0,"7":17352048.0,"8":16279532.0,"9":15683801.0,"10":16975831.0,"11":16986634.0,"12":17341962.0,"13":21286526.0},"BlockingAtomicQueue2":{"1":39681843.0,"2":14543387.0,"3":18757837.0,"4":19715309.0,"5":20939894.0,"6":20621634.0,"7":12647499.0,"8":15973301.0,"9":15591109.0,"10":16220540.0,"11":16370119.0,"12":18422462.0,"13":17183451.0},"boost::lockfree::queue":{"1":2832410.0,"2":2447314.0,"3":2313702.0,"4":2246386.0,"5":1869494.0,"6":1960557.0,"7":1160872.0,"8":875150.0,"9":857813.0,"10":947622.0,"11":851373.0,"12":710111.0,"13":719645.0},"pthread_spinlock":{"1":10528950.0,"2":4893893.0,"3":3415445.0,"4":1815364.0,"5":1838567.0,"6":1976192.0,"7":1356570.0,"8":750588.0,"9":406777.0,"10":488545.0,"11":938401.0,"12":771721.0,"13":880304.0},"tbb::concurrent_bounded_queue":{"1":6098542.0,"2":5424954.0,"3":3894589.0,"4":3475549.0,"5":3135563.0,"6":2789218.0,"7":1484382.0,"8":1068659.0,"9":1029527.0,"10":1041493.0,"11":1213040.0,"12":1157905.0,"13":1321028.0},"tbb::speculative_spin_mutex":{"1":30907825.0,"2":13545496.0,"3":9674606.0,"4":6905555.0,"5":7253715.0,"6":4233654.0,"7":2218420.0,"8":2033093.0,"9":1420848.0,"10":1388596.0,"11":1281117.0,"12":1205642.0,"13":1094144.0},"tbb::spin_mutex":{"1":34681551.0,"2":17158264.0,"3":10342594.0,"4":7667926.0,"5":3152852.0,"6":1965263.0,"7":983530.0,"8":676976.0,"9":448862.0,"10":406257.0,"11":370267.0,"12":361317.0,"13":296267.0}};

    plot('scalability-7700k-5GHz', results_to_series(results_7700k), "Intel 7700k (core 5GHz / uncore 4.7GHz)");
    plot('scalability-xeon-gold-6132', results_to_series(results_xeon_gold_6132), "Intel Xeon Gold 6132 (stock)");
});
