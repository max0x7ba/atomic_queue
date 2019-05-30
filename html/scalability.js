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
        Highcharts.chart('scalability-7700k-5GHz', {
            title: {
                text: 'Scalability on ' + title_suffix
            },

            subtitle: {
                text: title_suffix
            },

            xAxis: {
                title: {
                    text: 'number of producers, number of consumers'
                },
                tickInterval: 1
            },
            yAxis: {
                title: {
                    text: 'msg/sec'
                }
            },
            legend: {
                layout: 'vertical',
                align: 'right',
                verticalAlign: 'middle'
            },

            plotOptions: {
                series: {
                    label: {
                        connectorAllowed: false
                    },
                }
            },

            tooltip: {
                useHTML: true,
                shared: true,
                headerFormat: '<span style="font-size: 10px; font-style: bold">{point.key} producers, {point.key} consumers</span><table>',
                pointFormat: '<tr><td style="color: {series.color}">{series.name}: </td>' +'<td style="text-align: right"><b>{point.y} msg/sec</b></td></tr>',
                footerFormat: '</table>'
            },

            series: series
        });
    }

    const results_7700k = {"AtomicQueue":{"1":27473115.0,"2":13780199.0,"3":13532478.0},"AtomicQueue2":{"1":25092312.0,"2":14274779.0,"3":12566061.0},"BlockingAtomicQueue":{"1":117895405.0,"2":43575992.0,"3":36987120.0},"BlockingAtomicQueue2":{"1":98733131.0,"2":38478447.0,"3":37995297.0},"boost::lockfree::queue":{"1":10205501.0,"2":7970549.0,"3":7920727.0},"pthread_spinlock":{"1":41729933.0,"2":15577961.0,"3":14945822.0},"tbb::concurrent_bounded_queue":{"1":15349404.0,"2":15966472.0,"3":14038820.0},"tbb::speculative_spin_mutex":{"1":53445745.0,"2":39245530.0,"3":31189300.0},"tbb::spin_mutex":{"1":85232248.0,"2":54468218.0,"3":38036453.0}};

    const series_7700k = results_to_series(results_7700k);
    console.log(series_7700k);

    plot('scalability-7700k-5GHz', series_7700k, "Intel 7700k 5Ghz");

    // $.getJSON('scalability-7700k-5GHz.json', (data) => { console.log("loaded:", data); });
    // $.ajax({
    //     dataType: "json",
    //     utl: '//scalability-7700k-5GHz.json',
    //     success:(data) => { console.log("loaded:", data); }
    // });

});
