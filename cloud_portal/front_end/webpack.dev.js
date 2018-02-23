const fs = require('fs');
const webpack = require('webpack');
const merge = require('webpack-merge');
const common = require('./webpack.common.js');

module.exports = merge(common, {
    devtool: 'inline-source-map',
    devServer:{
        contentBase: './dist',
        hot: true,
        host: 'localhost',
        port: 9000,
        proxy: [
            {
                context: ['/api/', '/gateway/'],
                target: 'http://cloud-test.hdw.mx:80',

            },
            {
                context: '/static/',
                target: "https://localhost:9000",
                pathRewrite: {"^/static" : ""},
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
        new webpack.HotModuleReplacementPlugin()
    ]
});