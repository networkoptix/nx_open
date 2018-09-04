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
                context: ['/api/', '/gateway/'],
                target: 'http://cloud-test.hdw.mx:80',

            },
            {
                context: '/static/',
                target: 'https://0.0.0.0:9000',
                pathRewrite: {'^/static': ''},
                changeOrigin: true,
                secure: false
            }
        ],
        https:{
            key: fs.readFileSync('ssl_keys/server.key').toString(),
            cert: fs.readFileSync('ssl_keys/server.crt').toString(),
        },
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