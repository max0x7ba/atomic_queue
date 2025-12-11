'use strict';

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.


const theme_axis = {
    gridLineColor: '#444',
    labels: { style: { color: '#eee' } },
    title: { style: { color: '#eee' } }
};

Highcharts.theme = {
    chart: {
        backgroundColor: 'black',
        style: { fontFamily: "'Roboto Slab', sans-serif" }
    },
    title: {
        text: undefined,
        style: {
            color: 'white',
            fontSize: '20px'
        }
    },
    subtitle: { style: { color: '#aaa' } },
    xAxis: theme_axis,
    yAxis: theme_axis,
    tooltip: {
        followPointer: true,
        useHTML: true,
        backgroundColor: 'rgba(0, 0, 0, 0.8)',
        borderWidth: 1,
        borderColor: 'rgba(255, 255, 255, 0.5)',
        style: { color: 'white' }
    },
    plotOptions: {
        series: {
            pointPadding: 0.2,
            groupPadding: 0,
            borderWidth: 0,
            animationLimit: Infinity,
            // inactiveOtherPoints: false,
            stickyTracking: false,
            states: {
                inactive: {
                    // enabled: true,
                    opacity: 0.3,
                },
            }
        },
        boxplot: {
            medianColor: 'black',
            grouping: false,
            lineWidth: 1,
            stemWidth: 1,
            whiskerWidth: 3,
            borderWidth: 0,
        },
        column: {
            borderWidth: 0
        },
        bar: {
            grouping: false,
            borderWidth: 0
        },
        errorbar: {
            stemWidth: 1,
            whiskerLength: 3,
            lineWidth: 1,
            enableMouseTracking: false,
            showInLegend: false,
            color: "#aaa",
        },
    },
    legend: {
        layout: 'horizontal',
        align: 'left',
        verticalAlign: 'bottom',
        itemStyle: { color: '#eee' },
        itemHoverStyle: { color: 'white' },
        itemHiddenStyle: { color: '#888' }
    },
    lang: { thousandsSep: ',' },
    credits: { enabled: false },
    accessibility: { enabled: false }
};

// Apply the theme
Highcharts.setOptions(Highcharts.theme);
