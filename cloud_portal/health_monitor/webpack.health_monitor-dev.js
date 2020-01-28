const fs = require('fs');
const webpack = require('webpack');
const merge = require('webpack-merge');
const CopyWebpackPlugin = require('copy-webpack-plugin');
// const BundleAnalyzerPlugin = require('webpack-bundle-analyzer').BundleAnalyzerPlugin;
const common = require('./webpack.health_monitor.js');
const ExtractTextPlugin = require('extract-text-webpack-plugin');

const ENV = process.env.ENV = process.env.NODE_ENV = 'dev';
const host = '0.0.0.0';
const port = 9000;
const cloudInstance = process.env.server_address || process.env.CLOUD_INSTANCE || 'https://cloud-dev2.hdw.mx';
const localStatic = `https://${host}:${port}`;

module.exports = merge(common, {
    devtool  : 'cheap-module-eval-source-map',
    devServer: {
        contentBase       : './dist',
        hot               : true,
        host              : host,
        port              : port,
        proxy             : [
            // Uncomment to test local translation strings
            // {
            //     context: ['/api/utils/language'],
            //     target: localStatic,
            //     pathRewrite: {'^/api/utils/language': 'language_compiled.json'},
            //     changeOrigin: true,
            //     secure: false
            // },
            {
                context: [ '/api/', '/gateway/' ],
                target : cloudInstance,
                changeOrigin: true,
                secure: false

            },
            {
                context     : '/static/lang_en_US/',
                target      : cloudInstance,
                changeOrigin: true,
                secure      : false
            },
            {
                context: '/static/lang_ru_RU/',
                target: cloudInstance,
                changeOrigin: true,
                secure: false
            },
            {
                context: '/static/lang_de_DE/',
                target: cloudInstance,
                changeOrigin: true,
                secure: false
            },
            {
                context: '/static/lang_he_IL/',
                target: cloudInstance,
                changeOrigin: true,
                secure: false
            },
            {
                context: '/static/images/',
                target: cloudInstance,
                changeOrigin: true,
                secure: false
            },
            {
                context     : '/static/',
                target      : localStatic,
                pathRewrite : { '^/static': '' },
                changeOrigin: true,
                secure      : false
            },
        ],
        https             : {
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
    plugins  : [
        new webpack.HotModuleReplacementPlugin(),
    ],
    output: {
        filename  : 'scripts/[name].js',
        publicPath: '/'
    },
    module   : {
        rules: [
            {
                test   : /\.scss$/,
                include: /src/,
                loaders: [ 'raw-loader', 'sass-loader' ]
            },
            {
                test: /\.s?css$/,
                exclude: /src/,
                use : ExtractTextPlugin.extract({
                    fallback: 'style-loader',
                    use     : [
                        {
                            loader : 'css-loader',
                            options: {
                                url      : false,
                                sourceMap: true
                            }
                        },
                        {
                            loader : 'postcss-loader',
                            options: {
                                url      : false,
                                sourceMap: 'inline'
                            }
                        },
                        {
                            loader : 'sass-loader',
                            options: {
                                url      : false,
                                sourceMap: true
                            }
                        }
                    ]
                })
            }
        ]
    }
});
