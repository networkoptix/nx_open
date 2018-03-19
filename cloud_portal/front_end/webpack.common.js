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
  	entry:{
        app: 'scripts/entrypoint.js',
		webcommon: 'web_common/scripts/entrypoint.js'
	},
	plugins: [
        //Development plugins
		new CleanWebpackPlugin(['dist']),

        //Plugins used for making templates
        new webpack.NamedModulesPlugin(),
		new HtmlWebpackPlugin({
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
        	},
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
    resolve:{
        alias:{
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