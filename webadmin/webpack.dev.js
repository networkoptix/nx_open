const fs = require('fs');
const webpack = require('webpack');
const merge = require('webpack-merge');
const CopyWebpackPlugin = require('copy-webpack-plugin');
// const BundleAnalyzerPlugin = require('webpack-bundle-analyzer').BundleAnalyzerPlugin;
const server_address = process.env.server_address || 'https://10.1.5.115:7001';

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
                // target: 'https://fb7a19a3-2b0c-4feb-be48-539231e50113.relay.vmsproxy.hdw.mx/',
                target: server_address,
                changeOrigin: true,
                secure      : false
            },
            {
                context: '/static/',
                target: 'https://0.0.0.0:9000',
                pathRewrite: { '^/static' : '' },
                changeOrigin: true,
                secure: false
            }
        ],
        https: {
            spdy: {
                protocols: ['http/1.1']
            },
            key : fs.readFileSync('ssl_keys/server.key').toString(),
            cert: fs.readFileSync('ssl_keys/server.crt').toString()
        },
        historyApiFallback: {
            index: '/'
        }
    },
    plugins:[
        new webpack.HotModuleReplacementPlugin(),
        // new BundleAnalyzerPlugin({analyzerHost:'0.0.0.0', analyzerPort:9001})
    
        // make some resources available while serve the project locally
        new CopyWebpackPlugin([
            // Local customizations *********************
            {
                from: '../customization/',
                to: 'customization/'
            }
        ])
    ]
});
