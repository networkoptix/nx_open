const path = require('path');
const webpack = require('webpack');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const CleanWebpackPlugin = require('clean-webpack-plugin');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const ExtractTextPlugin = require("extract-text-webpack-plugin");

module.exports = {
    context: path.resolve(__dirname + '/app'),
    entry:{
        app: 'scripts/entrypoint-app.js',
        inline: 'scripts/entrypoint-inline.js',
        login: 'scripts/entrypoint-login.js',
        webcommonApp: 'web_common/scripts/entrypoint-app.js',
        webcommonPartial: 'web_common/scripts/entrypoint-partial.js',
        apiXsl: 'scripts/entrypoint-api-xsl.js',
    },
    plugins: [
        //Development plugins
        new CleanWebpackPlugin(['dist']),

        //Plugins used for making templates
        new webpack.NamedModulesPlugin(),
        new HtmlWebpackPlugin({
            chunks: ['commons', 'webcommonApp', 'app'],
            chunksSortMode: 'manual',
            template: 'index-template.html',
            filename: 'index.html',
            inject:false
        }),
        new HtmlWebpackPlugin({
            chunks: ['commons', 'webcommonPartial', 'inline'],
            chunksSortMode: 'manual',
            template: 'inline-template.html',
            filename: 'inline.html',
            inject:false
        }),
        new HtmlWebpackPlugin({
            chunks: ['commons', 'webcommonPartial', 'login'],
            chunksSortMode: 'manual',
            template: 'login-template.html',
            filename: 'login.html',
            inject:false
        }),
        new HtmlWebpackPlugin({
            chunks: ['commons', 'apiXsl'],
            template: 'api.xsl',
            filename: 'api.xsl',
            inject: false
        }),
        new ExtractTextPlugin({filename: 'styles/[name].css'}),
        new CopyWebpackPlugin([
            {
                from: '',
                to: '',
                ignore: ['web_common/**', 'styles/**', '.*', '*.js', '*-template.html']
            },
            {
                from: 'web_common',
                to: 'web_common',
                ignore: ['scripts/**', 'styles/**', 'chromeless/**', '*.js']
            },
            {
                from:'../node_modules/bootstrap-sass/assets/fonts',
                to: 'fonts'
            }
        ]),

        //Plugins for npm packages
        new webpack.ProvidePlugin({
            'md5': 'md5',
            '$': 'jquery',
            'jQuery': 'jquery',
            'window.jQuery': 'jquery',
            'QWebChannel': 'qwebchannel',
            '_': 'underscore',
            'Base64': 'base-64',
            'screenfull': 'screenfull'
        }),
        new webpack.optimize.CommonsChunkPlugin({
            name: "commons",
            filename: "scripts/commons.js",
            minChunks: 2
        })
    ],
    output: {
        filename: 'scripts/[name].js',
        path: path.resolve(__dirname, 'dist'),
        publicPath: ''
    },
    resolve:{
        alias:{
            fonts: path.join(__dirname, 'app', 'fonts'),
            scripts: path.join(__dirname, 'app', 'scripts'),
            web_common: path.join(__dirname, 'app', 'web_common')
        }
    },
    module: {
        loaders: [
            {
                test: /\.js$/,
                exclude: /node_modules/,
                loader: 'babel-loader'
            }
        ],
        rules: [
            {
                test: /\.(scss|css)$/,
                use: ExtractTextPlugin.extract({
                    use: [{
                        loader: "css-loader",
                        options:{
                            url: false
                        }
                    },
                    {
                        loader: "sass-loader",
                        options:{
                            url: false
                        }
                    }]
                })
            },
        ]
    }
};