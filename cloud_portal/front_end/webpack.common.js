const path = require('path');
const minimist = require('minimist');
const webpack = require('webpack');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const CleanWebpackPlugin = require('clean-webpack-plugin');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const ExtractTextPlugin = require("extract-text-webpack-plugin");

const webCommonPath = path.join(__dirname, '../../webadmin/app/web_common');

module.exports = {
    context: path.resolve(__dirname + '/app'),
    entry: {
        app: 'scripts/entrypoint.js',
        webcommon: 'web_common/scripts/entrypoint.js',
        polyfills: './polyfills.ts',
        vendor: './vendor.ts',
        appnew: './main.ts'
    },
    plugins: [
        //Development plugins
        new CleanWebpackPlugin(['dist']),

        new webpack.ContextReplacementPlugin(
            // The (\\|\/) piece accounts for path separators in *nix and Windows
            /*/angular(\\|\/)core(\\|\/)@angular/,*/
            // /\@angular(\\|\/)core(\\|\/)esm5/,
            /angular(\\|\/)core/,
            '/app', // location of your src
            {} // a map of your routes
        ),

        //Plugins used for making templates
        new webpack.NamedModulesPlugin(),
        new HtmlWebpackPlugin({
            title: 'Custom template',
            template: 'index-template.html',
            filename: 'index.html',
            inject: false
        }),
        new ExtractTextPlugin("styles/main.css"),
        new CopyWebpackPlugin([
            {
                from: '',
                to: '',
                ignore: ['bower_components/**', 'styles/**', '.*', '*.js', 'index-template.html', 'index.html']
            },
            {
                from: webCommonPath,
                to: 'web_common',
                ignore: ['scripts/**', 'styles/**', 'chromeless/**', '*.js']
            }
        ]),

        //Plugins for npm packages
        new webpack.ProvidePlugin({
            'md5': 'md5',
            '$': 'jquery',
            'jQuery': 'jquery',
            'window.jQuery': 'jquery',
            '_': 'underscore',
            'screenfull': 'screenfull',
            'jquery-mousewheel': 'jquery-mousewheel'
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
        publicPath: '/'
    },
    resolve: {
        extensions: ['.ts', '.js'],
        alias: {
            fonts: path.join(__dirname, 'app', 'fonts'),
            scripts: path.join(__dirname, 'app', 'scripts'),
            web_common: webCommonPath
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
                test: /\.ts$/,
                loaders: [
                    {
                        loader: 'awesome-typescript-loader',
                        options: {configFileName: 'tsconfig.json'}
                    }, 'angular2-template-loader'
                ]
            },
            {
                test: /\.html$/,
                include: /app/,
                exclude: /index-template\.html$/,
                use: ['html-loader']
            },
            {
                test: /\.(scss|css)$/,
                use: ExtractTextPlugin.extract({
                    use: [{
                        loader: "css-loader",
                        options: {
                            url: false
                        }
                    },
                        {
                            loader: "sass-loader",
                            options: {
                                url: false
                            }
                        }]
                })
            },
            {
                test: /\.(png|jpe?g|gif|svg|woff|woff2|ttf|eot|ico)$/,
                loader: 'file-loader?name=assets/[name].[hash].[ext]'
            }
        ]
    }
};
