const fs = require('fs');
const webpack = require('webpack');
const merge = require('webpack-merge');
const BundleAnalyzerPlugin = require('webpack-bundle-analyzer').BundleAnalyzerPlugin;

const common = require('./webpack.common.js');

module.exports = merge(common, {
    devtool: 'inline-source-map',
    devServer:{
        contentBase: './dist',
        hot: true,
        host: '0.0.0.0',
        port: 9000,
        proxy: [
            {
                context: ['/web/', '/api/', '/ec2/', '/hls/', '/media/', '/proxy/'],
                target: 'http://10.1.5.130:7001'
            },
            {
                context: '/static/',
                target: "http://0.0.0.0:9000",
                pathRewrite: {"^/static" : ""},
                changeOrigin: true,
                secure: false
            }
        ],
        historyApiFallback: {
            index: '/',
        },
    }
    ,
    plugins:[
        new webpack.HotModuleReplacementPlugin(),
        new BundleAnalyzerPlugin({analyzerHost:'0.0.0.0', analyzerPort:9001})

    ]
});